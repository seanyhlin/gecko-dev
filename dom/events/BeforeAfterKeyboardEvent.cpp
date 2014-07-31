/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#include "mozilla/dom/BeforeAfterKeyboardEvent.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/TextEvents.h"
#include "prtime.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Key", args);
#else
#define LOG(args...) printf(args);
#endif

namespace mozilla {
namespace dom {

BeforeAfterKeyboardEvent::BeforeAfterKeyboardEvent(EventTarget* aOwner,
                                                   nsPresContext* aPresContext,
                                                   WidgetKeyboardEvent* aEvent)
  : KeyboardEvent(aOwner, aPresContext,
                  aEvent ? aEvent : new WidgetBeforeAfterKeyboardEvent(false, 0, nullptr))
{
  if (!aEvent) {
    return;
  }

  MOZ_ASSERT(mEvent->eventStructType == NS_BEFORE_AFTER_KEY_EVENT,
             "event type mismatch NS_BEFORE_AFTER_EVENT");
  MOZ_ASSERT(mEvent->message == NS_KEY_BEFORE_DOWN ||
             mEvent->message == NS_KEY_BEFORE_UP ||
             mEvent->message == NS_KEY_AFTER_DOWN ||
             mEvent->message == NS_KEY_AFTER_UP,
             "event message mismatch");

  if (aEvent) {
    mEventIsInternal = false;

    if (mEvent->message == NS_KEY_AFTER_DOWN ||
        mEvent->message == NS_KEY_AFTER_UP) {
      bool value = aEvent->AsKeyboardEvent()->mFlags.mDefaultPrevented;
      mEmbeddedCancelled.SetValue(value);
      LOG("[BeforeAfterKeyboardEvent] mEmbeddedCancelled: %d", mEmbeddedCancelled.Value());
    }
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMBeforeAfterKeyboardEvent(nsIDOMEvent** aInstancePtrResult,
                                  EventTarget* aOwner,
                                  nsPresContext* aPresContext,
                                  WidgetKeyboardEvent *aEvent)
{
  LOG("NS_NewDOMBeforeAfterKeyboardEvent");
  BeforeAfterKeyboardEvent* it =
    new BeforeAfterKeyboardEvent(aOwner, aPresContext, aEvent);

  NS_ADDREF(it);
  *aInstancePtrResult = static_cast<Event*>(it);
  return NS_OK;
}
