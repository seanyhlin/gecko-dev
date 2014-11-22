#include "mozilla/dom/TabChild.h"
#include "NavigatorPresentationChild.h"

BEGIN_PRESENTATION_NAMESPACE

NavigatorPresentationChild::NavigatorPresentationChild()
  : mObserverList(PresentationEventObserverList())
{
  MOZ_COUNT_CTOR(NavigatorPresentationChild);
  nsRefPtr<TabChild> tabChild = static_cast<TabChild*>(Manager());
  if (tabChild) {
    tabChild->SendPNavigatorPresentationConstructor(this);
  }
}

NavigatorPresentationChild::~NavigatorPresentationChild()
{
  MOZ_COUNT_DTOR(NavigatorPresentationChild);
}

void
NavigatorPresentationChild::AddObserver(PresentationEventObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  mObserverList.AddObserver(aObserver);
}

void
NavigatorPresentationChild::RemoveObserver(PresentationEventObserver* aObserver)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  mObserverList.RemoveObserver(aObserver);
}

inline void
NavigatorPresentationChild::NotifyPresentationEvent(PresentationEvent aEvent)
{
  mObserverList.Broadcast(aEvent);
}

bool
NavigatorPresentationChild::RecvNotifyDeviceAvailableStatus(const bool& aAvailable)
{
  PresentationEvent pEvent;
  pEvent.eventType = EPresentationEvent::DeviceAvailableChanged;
  pEvent.available = aAvailable;
  NotifyPresentationEvent(pEvent);
  return true;
}

bool
NavigatorPresentationChild::RecvNotifyDeviceSeletedStatus(const nsCString& aOrigin,
                                                          const nsCString& aURL,
                                                          const nsCString& aSessionId,
                                                          const nsCString& aId,
                                                          const nsCString& aName,
                                                          const nsCString& aType)
{
  PresentationEvent pEvent;
  pEvent.eventType = EPresentationEvent::DeviceSelected;
  pEvent.deviceInfo.mOrigin = aOrigin;
  pEvent.deviceInfo.mURL = aURL;
  pEvent.deviceInfo.mSessionId = aSessionId;
  pEvent.deviceInfo.mId = aId;
  pEvent.deviceInfo.mName = aName;
  pEvent.deviceInfo.mType = aType;
  NotifyPresentationEvent(pEvent);
  return true;
}

bool
NavigatorPresentationChild::StartPresentationSession(const nsACString& aOrigin,
                                                     const nsACString& aURL,
                                                     const nsACString& aLastSessionId)
{
  return SendEstablishSession(nsCString(aOrigin),
                              nsCString(aURL),
                              nsCString(aLastSessionId));
}

bool
NavigatorPresentationChild::JoinPresentationSession(const nsACString& aOrigin,
                                                    const nsACString& aURL,
                                                    const nsACString& aLastSessionId)
{
  return SendResumeSession(nsCString(aOrigin),
                           nsCString(aURL),
                           nsCString(aLastSessionId));
}

END_PRESENTATION_NAMESPACE
