/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_PresentationSession_h
#define mozilla_dom_presentation_PresentationSession_h

#include "mozilla/DOMEventTargetHelper.h"
// Include PresentationSessionBinding.h since enum PresentationSessionState
// can't be forward declared.
#include "mozilla/dom/PresentationSessionBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIPresentationRequestCallback.h"

namespace mozilla {
namespace dom {

namespace presentation {

class PresentationSession MOZ_FINAL : public DOMEventTargetHelper
                                    , public nsIPresentationSessionListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationSession,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER

  static already_AddRefed<PresentationSession> Create(nsPIDOMWindow* aWindow,
                                                      const nsAString& aId);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsresult SetState(bool aState);

  // WebIDL (public APIs)

  void GetId(nsAString& aId) const;

  bool State() const;

  void Send(const nsAString& aData,
            ErrorResult& aRv);

  void Disconnect(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(statechange);
  IMPL_EVENT_HANDLER(message);

private:
  PresentationSession(nsPIDOMWindow* aWindow,
                      const nsAString& aId);

  ~PresentationSession();

  bool Init();

  void Shutdown();

  nsresult DispatchMessageEvent(JS::Handle<JS::Value> aData);

  nsString mId;
  bool mState;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_PresentationSession_h
