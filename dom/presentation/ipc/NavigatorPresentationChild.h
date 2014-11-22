
#ifndef mozilla_dom_NavigatorPresentationChild_h
#define mozilla_dom_NavigatorPresentationChild_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/PNavigatorPresentationChild.h"
#include "mozilla/dom/PresentationCommon.h"
#include "mozilla/dom/PresentationManager.h"
#include "mozilla/dom/PresentationSession.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"

BEGIN_PRESENTATION_NAMESPACE

class NavigatorPresentationChild : public IPresentationManager
                                 , public PNavigatorPresentationChild
{
  friend class PresentationSession;
public:
  // PNavigatorPresentationChild
  virtual bool
  RecvNotifyDeviceAvailableStatus(const bool& aAvailable);
  virtual bool
  RecvNotifyDeviceSeletedStatus(const nsCString& aOrigin,
                                const nsCString& aURL,
                                const nsCString& aSessionId,
                                const nsCString& aId,
                                const nsCString& aName,
                                const nsCString& aType);
  // IPresentationManager
  virtual bool
  StartPresentationSession(const nsACString& aOrigin,
                           const nsACString& aURL,
                           const nsACString& aLastSessionId) MOZ_OVERRIDE;
  // IPresentationManager
  virtual bool
  JoinPresentationSession(const nsACString& aOrigin,
                          const nsACString& aURL,
                          const nsACString& aLastSessionId) MOZ_OVERRIDE;
  NavigatorPresentationChild();
  virtual ~NavigatorPresentationChild() MOZ_OVERRIDE;

  virtual void
  AddObserver(PresentationEventObserver* aObserver) MOZ_OVERRIDE;
  virtual void
  RemoveObserver(PresentationEventObserver* aObserver) MOZ_OVERRIDE;

private:
  inline void NotifyPresentationEvent(PresentationEvent aEvent);

private:
  PresentationEventObserverList mObserverList;
};

END_PRESENTATION_NAMESPACE

#endif
