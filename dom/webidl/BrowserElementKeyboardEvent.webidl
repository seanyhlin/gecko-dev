/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface WindowProxy;

interface BrowserElementKeyboardEvent : KeyboardEvent
{
  // Valid value of embeddedCancelled:
  // - "mozbrowserbeforekeydown": null
  // - "mozbrowserbeforekeyup": null
  // - "mozbrowserafterkeydown": true/false
  // - "mozbrowserafterkeyup": true/false
  readonly attribute boolean? embeddedCancelled;
};

dictionary BrowserElementKeyboardEventInit : KeyboardEventInit
{
  boolean? embeddedCancelled = null;
};

