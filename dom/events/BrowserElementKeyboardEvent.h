/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#ifndef mozilla_dom_BrowserElementKeyboardEvent_h_
#define mozilla_dom_BrowserElementKeyboardEvent_h_

#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/BrowserElementKeyboardEventBinding.h"

class nsPresContext;

namespace mozilla {
namespace dom {

class BrowserElementKeyboardEvent : public KeyboardEvent
{
public:
  BrowserElementKeyboardEvent(EventTarget* aOwner,
                              nsPresContext* aPresContext,
                              WidgetKeyboardEvent* aEvent);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return BrowserElementKeyboardEventBinding::Wrap(aCx, this);
  }

  Nullable<bool> GetEmbeddedCancelled() const
  {
    return mEmbeddedCancelled;
  }

private:
  Nullable<bool> mEmbeddedCancelled;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BrowserElementKeyboardEvent_h_
