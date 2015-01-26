/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Session.h"

#include "nsIServerSocket.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsINetAddr.h"
#include "nsXPCOMCID.h"
#include "nsNetCID.h"
#include "nsISupportsPrimitives.h"
#include "nsIDNSService.h"
#include "nsIDOMEventListener.h"
#include "nsISocketTransport.h"
#include "nsISocketTransportService.h"
#include "nsISimpleEnumerator.h"
#include "nsITimerCallback.h"
#include "PresentationSessionTransport.h"
#include "PresentationService.h"

namespace mozilla {
namespace dom {
namespace presentation {

namespace {

class PresentationChannelDescription MOZ_FINAL : public nsIPresentationChannelDescription
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONCHANNELDESCRIPTION

  explicit PresentationChannelDescription(nsACString& aAddress, const uint16_t aPort)
    : mAddress(aAddress)
    , mPort(aPort)
  {
  }

private:
  virtual ~PresentationChannelDescription();

  nsCString mAddress;
  uint16_t mPort;
};

NS_IMPL_ISUPPORTS(PresentationChannelDescription, nsIPresentationChannelDescription)

PresentationChannelDescription::~PresentationChannelDescription()
{
}

// nsIPresentationChannelDescription
NS_IMETHODIMP
PresentationChannelDescription::GetType(uint8_t* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  // support only TCP socket now.
  *aRetVal = nsIPresentationChannelDescription::TYPE_TCP;

  return NS_OK;
}

NS_IMETHODIMP
PresentationChannelDescription::GetTcpAddress(nsIArray** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);

  nsCOMPtr<nsISupportsCString> address = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID);
  address->SetData(mAddress);

  array->AppendElement(address, false);
  array.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
PresentationChannelDescription::GetTcpPort(uint16_t* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);

  *aRetVal = mPort;

  return NS_OK;
}

NS_IMETHODIMP
PresentationChannelDescription::GetDataChannelSDP(nsAString& aDataChannelSDP)
{
  // support only TCP socket now.
  //aDataChannelSDP.Clear();

  return NS_OK;
}

// Session object with sender side behavior
class Requester MOZ_FINAL : public Session
                          , public nsIPresentationControlChannelListener
                          , public nsIServerSocketListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSISERVERSOCKETLISTENER

  explicit Requester(const nsAString& aId,
                     PresentationService* aService,
                     nsIPresentationControlChannel* aControlChannel)
    : Session(aId, aService)
    , mControlChannel(aControlChannel)
  {
  }

private:
  virtual ~Requester() {}

  nsCOMPtr<nsIServerSocket> mServerSocket;
  nsCOMPtr<nsIPresentationControlChannel> mControlChannel;
};

NS_IMPL_ISUPPORTS_INHERITED(Requester,
                            Session,
                            nsIPresentationControlChannelListener,
                            nsIServerSocketListener)

// nsIPresentationControlChannelListener
NS_IMETHODIMP
Requester::NotifyOpened()
{
  // XXX do nothing and waiting for receiver ready?
  return NS_OK;
}

NS_IMETHODIMP
Requester::NotifyClosed(nsresult aReason)
{
  // release control channel
  mControlChannel->SetListener(nullptr);
  mControlChannel = nullptr;

  // notify session failure if any
  if (NS_FAILED(aReason)) {
    OnClose(aReason);
  }

  // release server socket
  if (mServerSocket) {
    mServerSocket->Close();
    mServerSocket = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
Requester::NotifyReceiverReady()
{
  // prepare server socket for bootstrapping session channel
  mServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
  mServerSocket->Init(-1, true, -1);
  mServerSocket->AsyncListen(this);

  // prepare offer and send to remote endpoint
  int32_t serverPort;
  mServerSocket->GetPort(&serverPort);

  //XXX Does it really work?
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  nsCString host;
  dns->GetMyHostName(host);
      
  nsRefPtr<PresentationChannelDescription> offer =
    new PresentationChannelDescription(host, static_cast<uint16_t>(serverPort));
  mControlChannel->SendOffer(offer);

  return NS_OK;
}

NS_IMETHODIMP
Requester::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "sender side should not receive offer");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Requester::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(!mTransport);
  // PresentationSessionTransport would take care of firing sessionComplete callback
  return NS_OK;
}

// nsIServerSocketListener
NS_IMETHODIMP
Requester::OnSocketAccepted(nsIServerSocket* aServerSocket,
                            nsISocketTransport* aTransport)
{
  mTransport = new PresentationSessionTransport(aTransport, this);

  return NS_OK;
}

NS_IMETHODIMP
Requester::OnStopListening(nsIServerSocket* aServerSocket,
                           nsresult aStatus)
{
  //XXX need extra handle?
  return NS_OK;
}

// Session object with receiver side behavior
class Responder MOZ_FINAL : public Session
                          , public nsIPresentationControlChannelListener
                          , public nsIDOMEventListener
                          , public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSIDOMEVENTLISTENER

  explicit Responder(const nsAString& aId,
                     PresentationService* aService,
                     nsIPresentationControlChannel* aControlChannel)
    : Session(aId, aService)
    , mControlChannel(aControlChannel)
  {
  }

private:
  virtual ~Responder() {}

  nsCOMPtr<nsIPresentationControlChannel> mControlChannel;
};

NS_IMPL_ISUPPORTS_INHERITED0(Responder,
                             Session)

// nsIPresentationControlChannelListener
NS_IMETHODIMP
Responder::NotifyOpened()
{
  // TODO open app frame

  // XXX if frame opened successfully
  mControlChannel->ReceiverReady();

  return NS_OK;
}

NS_IMETHODIMP
Responder::NotifyClosed(nsresult aReason)
{
  // release control channel
  mControlChannel->SetListener(nullptr);
  mControlChannel = nullptr;

  // notify session failure if any
  if (NS_FAILED(aReason)) {
    OnClose(aReason);
  }

  return NS_OK;
}

NS_IMETHODIMP
Responder::NotifyReceiverReady()
{
  MOZ_ASSERT(false, "receiver side don't need session ready");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Responder::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  uint16_t serverPort;
  aDescription->GetTcpPort(&serverPort);

  nsCOMPtr<nsIArray> serverHosts;
  aDescription->GetTcpAddress(getter_AddRefs(serverHosts));

  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  serverHosts->Enumerate(getter_AddRefs(enumerator));
  bool more;
  enumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> isupport;
    enumerator->GetNext(getter_AddRefs(isupport));
    nsCOMPtr<nsISupportsCString> supportStr = do_QueryInterface(isupport);

    nsCString serverHost;
    supportStr->GetData(serverHost);

    nsCOMPtr<nsISocketTransport> socket;
    sts->CreateTransport(nullptr, 0, serverHost,
                         serverPort, nullptr, getter_AddRefs(socket));
    if (!socket) {
      // try next address
      enumerator->HasMoreElements(&more);
      continue;
    }

    mTransport = new PresentationSessionTransport(socket, this);

    nsCOMPtr<nsINetAddr> selfAddr;
    socket->GetScriptableSelfAddr(getter_AddRefs(selfAddr));
    
    nsCString host;
    selfAddr->GetAddress(host);
    uint16_t port;
    selfAddr->GetPort(&port);
    nsRefPtr<PresentationChannelDescription> answer = new PresentationChannelDescription(host, port);
    mControlChannel->SendAnswer(answer);
    break;
  }

  if (!mTransport) {
    // abort the entire procedure if cannot establish session transport
    mControlChannel->Close(NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
Responder::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "receiver side should not receive answer");
  return NS_ERROR_FAILURE;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS(Session,
                  nsIPresentationSessionTransportCallback);

/* static */ already_AddRefed<Session>
Session::CreateRequester(const nsAString& aId,
                         PresentationService* aService,
                         nsIPresentationControlChannel* aControlChannel)
{
  nsRefPtr<Requester> requester = new Requester(aId, aService, aControlChannel);

  return requester.forget();
}

/* static */ already_AddRefed<Session>
Session::CreateResponder(const nsAString& aId,
                         PresentationService* aService,
                         nsIPresentationControlChannel* aControlChannel)
{
  nsRefPtr<Responder> responder = new Responder(aId, aService, aControlChannel);

  return responder.forget();
}

void
Session::Send(nsIInputStream* aData)
{
  MOZ_ASSERT(mTransport);

  mTransport->Send(aData);
}

void
Session::Close(nsresult aReason)
{
  if (mTransport) {
    mTransport->Close(aReason);
  }
}

// nsIPresentationSessionTransportCallback
NS_IMETHODIMP
Session::OnComplete()
{
  mService->OnSessionComplete(this);
  return NS_OK;
}

NS_IMETHODIMP
Session::OnData(const nsACString& aData)
{
  mService->OnSessionMessage(this, aData);
  return NS_OK;
}

NS_IMETHODIMP
Session::OnClose(nsresult aReason)
{
  mService->OnSessionClose(this, aReason);
  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
