
#ifndef mozilla_dom_NavigatorPresentationParent_h
#define mozilla_dom_NavigatorPresentationParent_h

#include "mozilla/dom/PNavigatorPresentationParent.h"
#include "mozilla/dom/PresentationCommon.h"

BEGIN_PRESENTATION_NAMESPACE

class NavigatorPresentationParent : public PNavigatorPresentationParent
                                  , public PresentationEventObserver
{
public:
  // PNavigatorPresentationParent
  virtual bool
  RecvEstablishSession(const nsCString& aOrigin,
                       const nsCString& aUrl,
                       const nsCString& aSessionId);
  virtual bool
  RecvResumeSession(const nsCString& aOrigin,
                    const nsCString& aURL,
                    const nsCString& aLastSessionId);
  virtual void
  ActorDestroy(ActorDestroyReason aWhy);

  NavigatorPresentationParent();
  virtual ~NavigatorPresentationParent();

  // PresentaionEventObserver
  virtual void
  Notify(const PresentationEvent& aEvent) MOZ_OVERRIDE;
};

END_PRESENTATION_NAMESPACE

#endif
