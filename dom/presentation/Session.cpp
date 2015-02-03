/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Session.h"

#include "nsIServerSocket.h"
#include "nsIFrameLoader.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsINetAddr.h"
#include "nsXPCOMCID.h"
#include "nsNetCID.h"
#include "nsISupportsPrimitives.h"
#include "nsIDNSService.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsISocketTransport.h"
#include "nsISocketTransportService.h"
#include "nsISimpleEnumerator.h"
#include "nsITimer.h"
#include "PresentationSessionTransport.h"
#include "PresentationService.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIPresentationSessionRequest.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Presentation", args);
#else
#define LOG(args...)  printf(args);
#endif

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkManager.h"
#endif

using namespace mozilla::services;

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
    mControlChannel->SetListener(this);
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

NS_IMETHODIMP
Requester::NotifyOpened()
{
  LOG("[Requester] %s", __FUNCTION__);
  // XXX do nothing and waiting for receiver ready?
  return NS_OK;
}

NS_IMETHODIMP
Requester::NotifyClosed(nsresult aReason)
{
  LOG("[Requester] %s", __FUNCTION__);
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
  LOG("[Requester] %s", __FUNCTION__);
  // prepare server socket for bootstrapping session channel
  mServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
  mServerSocket->Init(-1, false, -1);
  mServerSocket->AsyncListen(this);

  // prepare offer and send to remote endpoint
  int32_t serverPort;
  mServerSocket->GetPort(&serverPort);

  nsCString host;
#ifdef MOZ_WIDGET_GONK
  nsCOMPtr<nsINetworkManager> networkManager =
    do_GetService("@mozilla.org/network/manager;1");
  nsCOMPtr<nsINetworkInterface> active;
  networkManager->GetActive(getter_AddRefs(active));
  if (NS_WARN_IF(!active)) {
    mControlChannel->Close(NS_ERROR_FAILURE);
    return NS_OK;
  }
  char16_t **ips = nullptr;
  uint32_t *prefixs = nullptr;
  uint32_t count = 0;
  active->GetAddresses(&ips, &prefixs, &count);
  if (NS_WARN_IF(!count)) {
    nsMemory::Free(prefixs);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, ips);
    mControlChannel->Close(NS_ERROR_FAILURE);
    return NS_OK;
  }
  nsAutoString ip;
  ip.Assign(ips[0]);
  host = NS_ConvertUTF16toUTF8(ip);

  nsMemory::Free(prefixs);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, ips);
#else
  //XXX Does it really work?
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  dns->GetMyHostName(host);
#endif
      
  nsRefPtr<PresentationChannelDescription> offer =
    new PresentationChannelDescription(host, static_cast<uint16_t>(serverPort));
  mControlChannel->SendOffer(offer);

  return NS_OK;
}

NS_IMETHODIMP
Requester::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  LOG("[Requester] %s", __FUNCTION__);
  MOZ_ASSERT(false, "sender side should not receive offer");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Requester::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  LOG("[Requester] %s", __FUNCTION__);
//  MOZ_ASSERT(!mTransport);
  // PresentationSessionTransport would take care of firing sessionComplete callback
  return NS_OK;
}

// nsIServerSocketListener
NS_IMETHODIMP
Requester::OnSocketAccepted(nsIServerSocket* aServerSocket,
                            nsISocketTransport* aTransport)
{
  LOG("[Requester] %s", __FUNCTION__);
  mTransport = new PresentationSessionTransport(aTransport, this, /* server */ true);

  return NS_OK;
}

NS_IMETHODIMP
Requester::OnStopListening(nsIServerSocket* aServerSocket,
                           nsresult aStatus)
{
  LOG("[Requester] %s", __FUNCTION__);
  //XXX need extra handle?
  return NS_OK;
}

#define DOMCONTENTLOADED_EVENT_NAME NS_LITERAL_STRING("DOMContentLoaded")

// Session object with receiver side behavior
class Responder MOZ_FINAL : public Session
                          , public nsIPresentationControlChannelListener
                          , public nsIDOMEventListener
                          , public nsITimerCallback
                          , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  explicit Responder(const nsAString& aId,
                     PresentationService* aService,
                     nsIPresentationControlChannel* aControlChannel)
    : Session(aId, aService)
    , mControlChannel(aControlChannel)
    , mTarget(nullptr)
    , mTimer(nullptr)
    , isLoaded(false)
  {
    Init();
  }

private:
  virtual ~Responder() {}

  bool Init();

  nsCOMPtr<nsIPresentationControlChannel> mControlChannel;
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsITimer> mTimer;
  bool isLoaded;
};

NS_IMPL_ISUPPORTS_INHERITED(Responder,
                            Session,
                            nsIObserver,
                            nsITimerCallback,
                            nsIDOMEventListener,
                            nsIPresentationControlChannelListener)

bool
Responder::Init()
{
  LOG("[Responder] %s", __FUNCTION__);
  mControlChannel->SetListener(this);
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  obs->AddObserver(this, "presentation-receiver-launched", false);
  return true;
}

// nsIObserver
NS_IMETHODIMP
Responder::Observe(nsISupports* aSubject,
                   const char* aTopic,
                   const char16_t* aData)
{
  LOG("[Responder] %s, %s", __FUNCTION__, aTopic);
  if (!strcmp(aTopic, "presentation-receiver-launched")) {
    nsCOMPtr<nsIFrameLoaderOwner> flOwner = do_QueryInterface(aSubject);
    nsCOMPtr<nsIFrameLoader> frameLoader;
    
    if (!flOwner ||
        NS_FAILED(flOwner->GetFrameLoader(getter_AddRefs(frameLoader))) ||
        NS_FAILED(frameLoader->ActivateFrameEvent(DOMCONTENTLOADED_EVENT_NAME, false))) {
      NS_WARNING("Failed to activate frame event");
      return NS_OK;
    }

    mTarget = do_QueryInterface(aSubject);
    if (NS_WARN_IF(!mTarget)) {
      return NS_OK;
    }
    mTarget->AddEventListener(DOMCONTENTLOADED_EVENT_NAME, this, false);

    nsresult rv;
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      mTimer->InitWithCallback(this, 10000, nsITimer::TYPE_ONE_SHOT);
    }

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "presentation-receiver-launched");
    }

    return NS_OK;
  }

  MOZ_ASSERT(false, "Unexpected topic for Responder.");
  return NS_ERROR_UNEXPECTED;
}

// nsITimerCallback
NS_IMETHODIMP
Responder::Notify(nsITimer* aTimer)
{
  LOG("[Responder] %s", __FUNCTION__);
  mTimer = nullptr;
  if (!isLoaded) {
    // timeout
    mControlChannel->Close(NS_ERROR_FAILURE);
  }
  return NS_OK;
}


// nsIPresentationControlChannelListener
NS_IMETHODIMP
Responder::NotifyOpened()
{
  LOG("[Responder] %s", __FUNCTION__);
  //XXX do nothing and wait for event DOMContentLoaded
  return NS_OK;
}

NS_IMETHODIMP
Responder::NotifyClosed(nsresult aReason)
{
  LOG("[Responder] %s", __FUNCTION__);
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
  LOG("[Responder] %s", __FUNCTION__);
  MOZ_ASSERT(false, "receiver side don't need session ready");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Responder::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  LOG("[Responder] %s", __FUNCTION__);
  uint16_t serverPort;
  aDescription->GetTcpPort(&serverPort);

  nsCOMPtr<nsIArray> serverHosts;
  aDescription->GetTcpAddress(getter_AddRefs(serverHosts));

  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);

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
    
    nsCString host = NS_LITERAL_CSTRING("127.0.0.1");
    //selfAddr->GetAddress(host);*/
    uint16_t port = 5566;
    //selfAddr->GetPort(&port);
    nsRefPtr<PresentationChannelDescription> answer = new PresentationChannelDescription(host, port);
    mControlChannel->SendAnswer(answer);
    break;
  }

  if (!mTransport) {
    // abort the entire procedure if cannot establish session transport
    // XXX NS_OK / NS_ERROR_FAILURE?
    mControlChannel->Close(NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
Responder::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  LOG("[Responder] %s", __FUNCTION__);
  MOZ_ASSERT(false, "receiver side should not receive answer");
  return NS_ERROR_FAILURE;
}

// nsIDOMEventListener
NS_IMETHODIMP
Responder::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  LOG("[Responder] %s", __FUNCTION__);
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  if (!isLoaded) {
    mControlChannel->ReceiverReady();
    isLoaded = true;
    mService->NotifySessionReady(Id());
  }

  if (mTarget) {
    mTarget->RemoveEventListener(DOMCONTENTLOADED_EVENT_NAME, this, false);
  }
  return NS_OK;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS(Session,
                  nsIPresentationSessionTransportCallback);

/* static */ already_AddRefed<Session>
Session::CreateRequester(const nsAString& aId,
                         PresentationService* aService,
                         nsIPresentationControlChannel* aControlChannel)
{
  LOG("[Session] %s", __FUNCTION__);
  nsRefPtr<Requester> requester = new Requester(aId, aService, aControlChannel);

  return requester.forget();
}

/* static */ already_AddRefed<Session>
Session::CreateResponder(const nsAString& aId,
                         PresentationService* aService,
                         nsIPresentationControlChannel* aControlChannel)
{
  LOG("[Session] %s", __FUNCTION__);
  nsRefPtr<Responder> responder = new Responder(aId, aService, aControlChannel);

  return responder.forget();
}

nsresult
Session::Send(nsIInputStream* aData)
{
  LOG("[Session] %s", __FUNCTION__);
  NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_AVAILABLE);

  return mTransport->Send(aData);
}

nsresult
Session::Close(nsresult aReason)
{
  LOG("[Session] %s", __FUNCTION__);
  if (mTransport) {
    return mTransport->Close(aReason);
  }

  return NS_OK;
}

// nsIPresentationSessionTransportCallback
NS_IMETHODIMP
Session::OnComplete()
{
  LOG("[Session] %s", __FUNCTION__);
  mService->OnSessionComplete(this);
  return NS_OK;
}

NS_IMETHODIMP
Session::OnData(const nsACString& aData)
{
  LOG("[Session] %s", __FUNCTION__);
  mService->OnSessionMessage(this, aData);
  return NS_OK;
}

NS_IMETHODIMP
Session::OnClose(nsresult aReason)
{
  LOG("[Session] %s", __FUNCTION__);
  mService->OnSessionClose(this, aReason);
  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
