/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#include "mozilla/dom/BrowserElementKeyboardEvent.h"
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

BrowserElementKeyboardEvent::BrowserElementKeyboardEvent(EventTarget* aOwner,
                                                         nsPresContext* aPresContext,
                                                         WidgetKeyboardEvent* aEvent)
  : KeyboardEvent(aOwner, aPresContext,
                  aEvent ? aEvent : new WidgetKeyboardEvent(false, 0, nullptr))
{
  MOZ_ASSERT(mEvent->eventStructType == NS_KEY_EVENT,
             "event type mismatch NS_KEY_EVENT");
  MOZ_ASSERT(mEvent->message == NS_KEY_BEFORE_DOWN ||
             mEvent->message == NS_KEY_BEFORE_UP ||
             mEvent->message == NS_KEY_AFTER_DOWN ||
             mEvent->message == NS_KEY_AFTER_UP,
             "event message mismatch");

  if (aEvent) {
    mEventIsInternal = false;

    nsAutoString typeString;
    switch (mEvent->message) {
      case NS_KEY_BEFORE_DOWN:
        typeString.AssignLiteral("mozbrowserbeforekeydown");
        break;
      case NS_KEY_BEFORE_UP:
        typeString.AssignLiteral("mozbrowserbeforekeyup");
        break;
      case NS_KEY_AFTER_DOWN:
        typeString.AssignLiteral("mozbrowserkeydown");
        break;
      case NS_KEY_AFTER_UP:
        typeString.AssignLiteral("mozbrowserkeyup");
        break;
      default:
        break;
    }
    mEvent->typeString = typeString;
    nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + typeString);
    mEvent->userType = atom;

    if (mEvent->message == NS_KEY_AFTER_DOWN ||
        mEvent->message == NS_KEY_AFTER_UP) {
      mEmbeddedCancelled.SetValue(aEvent->AsKeyboardEvent()->mFlags.mDefaultPrevented);
      LOG("[BrowserElementKeyboardEvent] mEmbeddedCancelled: %d", mEmbeddedCancelled.Value());
    }
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->AsKeyboardEvent()->mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
  }
}

// static
already_AddRefed<BrowserElementKeyboardEvent>
BrowserElementKeyboardEvent::Constructor(EventTarget* aOwner,
                                         const nsAString& aType,
                                         const BrowserElementKeyboardEventInit& aParam)
{
  nsRefPtr<BrowserElementKeyboardEvent> newEvent =
    new BrowserElementKeyboardEvent(aOwner, nullptr, nullptr);
  bool trusted = newEvent->Init(aOwner);
  newEvent->InitKeyEvent(aType, aParam.mBubbles, aParam.mCancelable,
                         aParam.mView, aParam.mCtrlKey, aParam.mAltKey,
                         aParam.mShiftKey, aParam.mMetaKey,
                         aParam.mKeyCode, aParam.mCharCode);
  newEvent->SetTrusted(trusted);

  return newEvent.forget();
}

// static
already_AddRefed<BrowserElementKeyboardEvent>
BrowserElementKeyboardEvent::Constructor(const GlobalObject& aGlobal,
                                         const nsAString& aType,
                                         const BrowserElementKeyboardEventInit& aParam,
                                         ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aParam);
}

uint32_t
BrowserElementKeyboardEvent::CharCode()
{
  if (mEvent->message == NS_KEY_PRESS) {
    return mEvent->AsKeyboardEvent()->charCode;
  }

  return 0;
}

uint32_t
BrowserElementKeyboardEvent::KeyCode()
{
  switch (mEvent->message) {
  case NS_KEY_BEFORE_DOWN:
  case NS_KEY_DOWN:
  case NS_KEY_AFTER_DOWN:
  case NS_KEY_PRESS:
  case NS_KEY_BEFORE_UP:
  case NS_KEY_UP:
  case NS_KEY_AFTER_UP:
    return mEvent->AsKeyboardEvent()->keyCode;
  }
  return 0;
}

uint32_t
BrowserElementKeyboardEvent::Which()
{
  switch (mEvent->message) {
    case NS_KEY_BEFORE_DOWN:
    case NS_KEY_DOWN:
    case NS_KEY_AFTER_DOWN:
    case NS_KEY_BEFORE_UP:
    case NS_KEY_UP:
    case NS_KEY_AFTER_UP:
      return KeyCode();
    case NS_KEY_PRESS:
      //Special case for 4xp bug 62878.  Try to make value of which
      //more closely mirror the values that 4.x gave for RETURN and BACKSPACE
      {
        uint32_t keyCode = mEvent->AsKeyboardEvent()->keyCode;
        if (keyCode == NS_VK_RETURN || keyCode == NS_VK_BACK) {
          return keyCode;
        }
        return CharCode();
      }
  }

  return 0;
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
  LOG("[BrowserElementKeyboardEvent] NS_NewDOMBrowserElementKeyboardEvent");
  BrowserElementKeyboardEvent *it =
    new BrowserElementKeyboardEvent(aOwner, aPresContext, aEvent);
    
  NS_ADDREF(it);
  *aInstancePtrResult = static_cast<Event*>(it);
  return NS_OK;
}
