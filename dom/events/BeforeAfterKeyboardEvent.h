/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#ifndef mozilla_dom_BeforeAfterKeyboardEvent_h_
#define mozilla_dom_BeforeAfterKeyboardEvent_h_

#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/BeforeAfterKeyboardEventBinding.h"

class nsPresContext;

namespace mozilla {
namespace dom {

class BeforeAfterKeyboardEvent : public KeyboardEvent
{
public:
  BeforeAfterKeyboardEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetKeyboardEvent* aEvent);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return BeforeAfterKeyboardEventBinding::Wrap(aCx, this);
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

#endif // mozilla_dom_BeforeAfterKeyboardEvent_h_
