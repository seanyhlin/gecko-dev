/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BeforeAfterKeyboardEvent.h"
#include "mozilla/TextEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

BeforeAfterKeyboardEvent::BeforeAfterKeyboardEvent(
                                       EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       InternalBeforeAfterKeyboardEvent* aEvent)
  : KeyboardEvent(aOwner, aPresContext,
                  aEvent ? aEvent :
                           new InternalBeforeAfterKeyboardEvent(false, 0,
                                                                nullptr))
{
  MOZ_ASSERT(mEvent->mClass == eBeforeAfterKeyboardEvent,
             "event type mismatch eBeforeAfterKeyboardEvent");
  MOZ_ASSERT(aEvent &&
             (aEvent->IsBeforeKeyEvent() || aEvent->IsAfterKeyEvent()),
             "event message mismatch");

  if (!aEvent) {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

// static
already_AddRefed<BeforeAfterKeyboardEvent>
BeforeAfterKeyboardEvent::Constructor(
                            EventTarget* aOwner,
                            const nsAString& aType,
                            const BeforeAfterKeyboardEventInit& aParam)
{
  nsRefPtr<BeforeAfterKeyboardEvent> event =
    new BeforeAfterKeyboardEvent(aOwner, nullptr, nullptr);
  ErrorResult rv;
  event->InitWithKeyboardEventInit(aOwner, aType, aParam, rv);
  NS_WARN_IF(rv.Failed());

  if (!aParam.mEmbeddedCancelled.IsNull()) {
    event->mEvent->AsBeforeAfterKeyboardEvent()->mEmbeddedCancelled =
      aParam.mEmbeddedCancelled.Value();
  }
  return event.forget();
}

// static
already_AddRefed<BeforeAfterKeyboardEvent>
BeforeAfterKeyboardEvent::Constructor(
                            const GlobalObject& aGlobal,
                            const nsAString& aType,
                            const BeforeAfterKeyboardEventInit& aParam,
                            ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aParam);
}

Nullable<bool>
BeforeAfterKeyboardEvent::GetEmbeddedCancelled() const
{
  Nullable<bool> embeddedCancelled;
  InternalBeforeAfterKeyboardEvent* internalEvent =
    mEvent->AsBeforeAfterKeyboardEvent();
  if (internalEvent->IsAfterKeyEvent()) {
    embeddedCancelled.SetValue(internalEvent->mEmbeddedCancelled); 
  }
  return embeddedCancelled;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMBeforeAfterKeyboardEvent(nsIDOMEvent** aInstancePtrResult,
                                  EventTarget* aOwner,
                                  nsPresContext* aPresContext,
                                  InternalBeforeAfterKeyboardEvent* aEvent)
{
  BeforeAfterKeyboardEvent* it =
    new BeforeAfterKeyboardEvent(aOwner, aPresContext, aEvent);

  NS_ADDREF(it);
  *aInstancePtrResult = static_cast<Event*>(it);
  return NS_OK;
}
