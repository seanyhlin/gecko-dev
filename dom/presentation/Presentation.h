/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_Presentation_h
#define mozilla_dom_presentation_Presentation_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsIPresentationRequestCallback.h"
#include "nsServiceManagerUtils.h"
#include "nsWrapperCache.h"
#include "PresentationSession.h"

struct JSContext;

namespace mozilla {
namespace dom {

class Promise;

namespace presentation {

class Presentation MOZ_FINAL : public DOMEventTargetHelper
                             , public nsIPresentationListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Presentation,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONLISTENER

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<Presentation>
  Create(nsPIDOMWindow* aWindow);

  Presentation(nsPIDOMWindow* aWindow);

  // WebIDL (public APIs)

  already_AddRefed<Promise>
  StartSession(const nsAString& aUrl,
               const Optional<nsAString>& aId,
               ErrorResult& aRv);

  already_AddRefed<Promise>
  JoinSession(const nsAString& aUrl,
              const nsAString& aId,
              ErrorResult& aRv);

  already_AddRefed<PresentationSession>
  GetSession() const;
  
  IMPL_EVENT_HANDLER(sessionready);

  bool
  Available() const;

  IMPL_EVENT_HANDLER(availablechange);

private:
  ~Presentation();

  bool Init();

  void Shutdown();

  bool mAvailable;
  nsRefPtr<PresentationSession> mSession;
};

} // namdspace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_Presentation_h
