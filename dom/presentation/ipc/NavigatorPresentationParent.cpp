#include "mozilla/dom/PresentationManager.h"
#include "NavigatorPresentationParent.h"

BEGIN_PRESENTATION_NAMESPACE

void
NavigatorPresentationParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

MOZ_IMPLICIT NavigatorPresentationParent::NavigatorPresentationParent()
{
  MOZ_COUNT_CTOR(NavigatorPresentationParent);
  PresentationManager::GetInstance()->AddObserver(this);
}

MOZ_IMPLICIT NavigatorPresentationParent::~NavigatorPresentationParent()
{
  MOZ_COUNT_DTOR(NavigatorPresentationParent);
  PresentationManager::GetInstance()->RemoveObserver(this);
}

bool
NavigatorPresentationParent::RecvEstablishSession(const nsCString& aOrigin,
                                                  const nsCString& aUrl,
                                                  const nsCString& aSessionId)
{
  PresentationManager::GetInstance()->StartPresentationSession(aOrigin,
                                                               aUrl,
                                                               aSessionId);
  return true;
}

bool
NavigatorPresentationParent::RecvResumeSession(const nsCString& aOrigin,
                                               const nsCString& aURL,
                                               const nsCString& aLastSessionId)
{
  PresentationManager::GetInstance()->JoinPresentationSession(aOrigin,
                                                              aURL,
                                                              aLastSessionId);

  return true;
}

void
NavigatorPresentationParent::Notify(const PresentationEvent& aEvent)
{
  if (aEvent.eventType == EPresentationEvent::DeviceAvailableChanged) {
    DebugOnly<bool> ret = SendNotifyDeviceAvailableStatus(aEvent.available);
    NS_WARN_IF_FALSE(ret, "SendNotifyDeviceAvailableStatus failed !!");
  } else if (aEvent.eventType == EPresentationEvent::DeviceSelected) {
    DebugOnly<bool> ret = SendNotifyDeviceSeletedStatus(aEvent.deviceInfo.mOrigin,
                                                        aEvent.deviceInfo.mURL,
                                                        aEvent.deviceInfo.mSessionId,
                                                        aEvent.deviceInfo.mId,
                                                        aEvent.deviceInfo.mName,
                                                        aEvent.deviceInfo.mType);
    NS_WARN_IF_FALSE(ret, "SendNotifyDeviceSeletedStatus failed !!");
  }
}

END_PRESENTATION_NAMESPACE
