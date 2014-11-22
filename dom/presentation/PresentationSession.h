/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSession_h
#define mozilla_dom_PresentationSession_h

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

class PresentationSession MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationSession,
                                           DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  PresentationSession(nsPIDOMWindow* aWindow);

  // WebIDL (public APIs)

  void GetId(nsAString& aId) const;

  uint32_t BufferedAmount() const;

  PresentationReadyState ReadyState() const;

  PresentationSessionState SessionState() const;

  PresentationBinaryType BinaryType() const;

  void SetBinaryType(PresentationBinaryType aBinaryType);

  IMPL_EVENT_HANDLER(message);
  IMPL_EVENT_HANDLER(open);
  IMPL_EVENT_HANDLER(close);
  IMPL_EVENT_HANDLER(error);
  IMPL_EVENT_HANDLER(sessionstatechange);

  void Send(const StringOrBlobOrArrayBufferOrArrayBufferView& data,
            ErrorResult& aRv);

  void Close(ErrorResult& aRv);

private:
  virtual ~PresentationSession();

  nsString mId;
  uint32_t mBufferedAmount;
  PresentationReadyState mReadyState;
  PresentationSessionState mSessionState;
  PresentationBinaryType mBinaryType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationSession_h
