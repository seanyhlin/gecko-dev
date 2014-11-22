/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NavigatorPresentation_h
#define mozilla_dom_NavigatorPresentation_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsRefPtrHashtable.h"
#include "nsWrapperCache.h"

#include "PresentationCommon.h"
#include "PresentationManager.h"
#include "PresentationSession.h"

struct JSContext;

BEGIN_PRESENTATION_NAMESPACE

class NavigatorPresentation MOZ_FINAL : public DOMEventTargetHelper
                                      , public PresentationEventObserver
                                      , public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NavigatorPresentation,
                                           DOMEventTargetHelper)
  NavigatorPresentation(nsPIDOMWindow* aWindow);
  void
  Init();

  void
  Shutdown();

  // PresentaionEventObserver
  virtual void
  Notify(const PresentationEvent& aEvent) MOZ_OVERRIDE;

  void
  NotifyAvailableChange(bool aAvailable);

  void
  NotifySessionInitInfo(const nsACString& aOrigin,
                        const nsACString& aURL,
                        const nsACString& aSessionId,
                        const nsACString& aDeviceId);

  nsresult
  AnswerPromise(const nsACString& aOrigin,
                const nsACString& aURL,
                const nsACString& aSessionId);

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const { return GetOwner(); }
  // WebIDL
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  // WebIDL
  already_AddRefed<Promise>
  StartSession(const nsAString& aUrl,
               const nsAString& aSessionId,
               ErrorResult& aRv);
  // WebIDL
  already_AddRefed<Promise>
  JoinSession(const nsAString& aUrl,
              const nsAString& aSessionId,
              ErrorResult& aRv);
  // WebIDL
  already_AddRefed<PresentationSession>
  GetSession();
  // WebIDL
  bool
  Available() const { return mAvailable; }
  // WebIDL
  IMPL_EVENT_HANDLER(availablechange);

private:
  ~NavigatorPresentation();

  void
  CreateIPresentationManager();

  void
  CreateSessionOnPresenter(const nsACString& aURL,
                           const nsACString& aSessionId,
                           const nsACString& aDeviceId);

  IPresentationManager* mPresentationManager;

  // Only be assigned on remote screen.
  nsRefPtr<PresentationSession> mSession;
  nsRefPtrHashtable<nsCStringHashKey, PresentationSession> mSessionStore;
  nsRefPtrHashtable<nsCStringHashKey, Promise> mPromiseStore;

  bool mAvailable;
  bool mIsPresenter;
};

END_PRESENTATION_NAMESPACE

#endif // mozilla_dom_NavigatorPresentation_h
