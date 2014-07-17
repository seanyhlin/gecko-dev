/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#include "mozilla/dom/BrowserElementKeyboardEvent.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/TextEvents.h"

namespace mozilla {
namespace dom {

BrowserElementKeyboardEvent::BrowserElementKeyboardEvent(
                                                    EventTarget* aOwner,
                                                    nsPresContext* aPresContext,
                                                    WidgetKeyboardEvent* aEvent)
  : KeyboardEvent(aOwner, aPresContext,
                  aEvent ? aEvent : new WidgetKeyboardEvent(false, 0, nullptr))
{
  if (!aEvent) {
    return;
  }

  MOZ_ASSERT(mEvent->eventStructType == NS_KEY_EVENT,
             "event type mismatch NS_KEY_EVENT");
  MOZ_ASSERT(mEvent->message == NS_KEY_BEFORE_DOWN ||
             mEvent->message == NS_KEY_BEFORE_UP ||
             mEvent->message == NS_KEY_AFTER_DOWN ||
             mEvent->message == NS_KEY_AFTER_UP,
             "event message mismatch");

  if (mEvent->message == NS_KEY_AFTER_DOWN ||
      mEvent->message == NS_KEY_AFTER_UP) {
    bool value = aEvent->AsKeyboardEvent()->mFlags.mDefaultPrevented;
    mEmbeddedCancelled.SetValue(value);
  }
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMBrowserElementKeyboardEvent(nsIDOMEvent** aInstancePtrResult,
                                     EventTarget* aOwner,
                                     nsPresContext* aPresContext,
                                     WidgetKeyboardEvent *aEvent)
{
  BrowserElementKeyboardEvent* it =
    new BrowserElementKeyboardEvent(aOwner, aPresContext, aEvent);

  NS_ADDREF(it);
  *aInstancePtrResult = static_cast<Event*>(it);
  return NS_OK;
}
