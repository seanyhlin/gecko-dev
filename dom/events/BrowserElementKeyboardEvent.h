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

  static already_AddRefed<BrowserElementKeyboardEvent>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const BrowserElementKeyboardEventInit& aParam,
              ErrorResult& aRv);

  static already_AddRefed<BrowserElementKeyboardEvent>
  Constructor(EventTarget* aOwner,
              const nsAString& aType,
              const BrowserElementKeyboardEventInit& aParam);

  Nullable<bool> GetEmbeddedCancelled() const
  {
    return mEmbeddedCancelled;
  }

  virtual uint32_t CharCode() MOZ_OVERRIDE;
  virtual uint32_t KeyCode() MOZ_OVERRIDE;
  virtual uint32_t Which() MOZ_OVERRIDE;

private:
  Nullable<bool> mEmbeddedCancelled;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BrowserElementKeyboardEvent_h_
