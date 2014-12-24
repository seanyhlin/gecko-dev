/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_PresentationSession_h
#define mozilla_dom_presentation_PresentationSession_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
namespace dom {
class StringOrBlobOrArrayBufferOrArrayBufferView;

namespace presentation {

class PresentationSession MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  explicit PresentationSession(nsPIDOMWindow* aWindow, const nsAString& aId);

  // WebIDL (public APIs)

  void GetId(nsAString& aId) const;

  PresentationSessionState State() const;

  IMPL_EVENT_HANDLER(statechange);

  void Send(const StringOrBlobOrArrayBufferOrArrayBufferView& data,
            ErrorResult& aRv);

  IMPL_EVENT_HANDLER(message);

  void Disconnect(ErrorResult& aRv);

private:
  virtual ~PresentationSession();

  nsString mId;
  PresentationSessionState mState;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_PresentationSession_h
