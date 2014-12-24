/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationService.h"
#include "PresentationSession.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "mozilla/dom/UnionTypes.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::presentation;

NS_IMPL_ADDREF_INHERITED(PresentationSession, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PresentationSession, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PresentationSession)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

PresentationSession::PresentationSession(nsPIDOMWindow* aWindow,
                                         const nsAString& aId)
  : DOMEventTargetHelper(aWindow)
  , mId(aId)
  , mState(PresentationSessionState::Disconnected)
{
}

/* virtual */ PresentationSession::~PresentationSession()
{
  PresentationService* service = PresentationService::Get();
  if (!service) {
    return;
  }
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

PresentationSessionState
PresentationSession::State() const
{
  return mState;
}

void
PresentationSession::Send(
  const StringOrBlobOrArrayBufferOrArrayBufferView& data,
  ErrorResult& aRv)
{
  // TODO Not implement yet.
}

void
PresentationSession::Disconnect(ErrorResult& aRv)
{
  // TODO Not implement yet.
}
