#include "Session.h"
#include "PresentationService.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

class Requester : public Session
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTCALLBACK

  explicit Requester(const nsAString& aId,
                     nsIPresentationControlChannel* aControlChannel,
                     PresentationService* aService);

  virtual ~Requester();
};

NS_IMPL_ISUPPORTS_INHERITED0(Requester, Session)

NS_IMETHODIMP
Requester::OnOffer(nsIPresentationChannelDescription*)
{
  return NS_OK;
}

NS_IMETHODIMP
Requester::OnAnswer(nsIPresentationChannelDescription*)
{
  return NS_OK;
}

NS_IMETHODIMP
Requester::NotifyOpened()
{
  return NS_OK;
}

NS_IMETHODIMP
Requester::NotifyClosed(tag_nsresult)
{
  return NS_OK;
}

NS_IMETHODIMP
Requester::OnComplete()
{
  mService->OnSessionComplete(this);
  return NS_OK;
}

NS_IMETHODIMP
Requester::OnClose(nsresult aReason)
{
  mService->OnSessionClose(this, aReason);
  return NS_OK;
}

NS_IMETHODIMP
Requester::OnMessage(const nsACString& aMessage)
{
  mService->OnSessionMessage(this, aMessage);
  return NS_OK;
}


Requester::Requester(const nsAString& aId,
                     nsIPresentationControlChannel* aControlChannel,
                     PresentationService* aService)
  : Session(aId, aControlChannel, aService)
{
}

Requester::~Requester()
{
}

class Responder : public Session
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONCONTROLCHANNELLISTENER
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORTCALLBACK

  explicit Responder(const nsAString& aId,
                     nsIPresentationControlChannel* aControlChannel,
                     PresentationService* aService);

  virtual ~Responder();
};

NS_IMPL_ISUPPORTS_INHERITED0(Responder, Session)

Responder::Responder(const nsAString& aId,
                     nsIPresentationControlChannel* aControlChannel,
                     PresentationService* aService)
  : Session(aId, aControlChannel, aService)
{
}

Responder::~Responder()
{
}

NS_IMETHODIMP
Responder::OnOffer(nsIPresentationChannelDescription*)
{
  return NS_OK;
}

NS_IMETHODIMP
Responder::OnAnswer(nsIPresentationChannelDescription*)
{
  return NS_OK;
}

NS_IMETHODIMP
Responder::NotifyOpened()
{
  return NS_OK;
}

NS_IMETHODIMP
Responder::NotifyClosed(tag_nsresult)
{
  return NS_OK;
}

NS_IMETHODIMP
Responder::OnComplete()
{
  mService->OnSessionComplete(this);
  return NS_OK;
}

NS_IMETHODIMP
Responder::OnClose(nsresult aReason)
{
  mService->OnSessionClose(this, aReason);
  return NS_OK;
}

NS_IMETHODIMP
Responder::OnMessage(const nsACString& aMessage)
{
  mService->OnSessionMessage(this, aMessage);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(Session,
                  nsIPresentationControlChannelListener,
                  nsIPresentationSessionTransportCallback)

Session::Session(const nsAString& aId,
                 nsIPresentationControlChannel* aControlChannel,
                 PresentationService* aService)
  : mId(aId)
  , mControlChannel(aControlChannel)
  , mService(aService)
{
}

Session::~Session()
{
  mControlChannel = nullptr;
  mService = nullptr;
}

/* static */ already_AddRefed<Session>
Session::CreateRequester(const nsAString& aId,
                         nsIPresentationControlChannel* aControlChannel,
                         PresentationService* aService)
{
  nsRefPtr<Requester> requester =
    new Requester(aId, aControlChannel, aService);
  return requester.forget();
}

/* static */ already_AddRefed<Session>
Session::CreateResponder(const nsAString& aId,
                         nsIPresentationControlChannel* aControlChannel,
                         PresentationService* aService)
{
  nsRefPtr<Responder> responder =
    new Responder(aId, aControlChannel, aService);
  return responder.forget();
}
