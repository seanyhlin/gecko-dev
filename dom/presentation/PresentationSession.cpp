/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationSession.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationSession, DOMEventTargetHelper)

// QueryInterface implementation for PresentationSession
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PresentationSession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationSession::PresentationSession(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mBufferedAmount(0)
  , mReadyState(PresentationReadyState::Closed)
  , mSessionState(PresentationSessionState::Disconnected)
  , mBinaryType(PresentationBinaryType::Blob)
{
}

/* virtual */ PresentationSession::~PresentationSession()
{
}

/* virtual */ JSObject*
PresentationSession::WrapObject(JSContext* aCx)
{
  return PresentationSessionBinding::Wrap(aCx, this);
}

void
PresentationSession::GetId(nsAString& aId) const
{
  aId = mId;
}

uint32_t
PresentationSession::BufferedAmount() const
{
  return mBufferedAmount;
}

PresentationSessionState
PresentationSession::SessionState() const
{
  return mSessionState;
}

PresentationReadyState
PresentationSession::ReadyState() const
{
  return mReadyState;
}

PresentationBinaryType
PresentationSession::BinaryType() const
{
  return mBinaryType;
}

void
PresentationSession::SetBinaryType(PresentationBinaryType aBinaryType)
{
  mBinaryType = aBinaryType;
}

void
PresentationSession::Send(
  const StringOrBlobOrArrayBufferOrArrayBufferView& data,
  ErrorResult& aRv)
{
  // TODO Not implement yet.
}

void
PresentationSession::Close(ErrorResult& aRv)
{
  // TODO Not implement yet.
}

} // namespace dom
} // namespace mozilla
