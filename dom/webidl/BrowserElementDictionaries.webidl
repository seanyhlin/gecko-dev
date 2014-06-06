/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

dictionary AsyncScrollEventDetail {
  float top = 0;
  float left = 0;
  float width = 0;
  float height = 0;
  float scrollWidth = 0;
  float scrollHeight = 0;
};

dictionary OpenWindowEventDetail {
  DOMString url = "";
  DOMString name = "";
  DOMString features = "";
  Node? frameElement = null;
};

dictionary BeforeKeyEventDetail {
  unsigned long charCode = 0;
  unsigned long keyCode = 0;

  boolean       altKey = false;
  boolean       ctrlKey = false;
  boolean       shiftKey = false;
  boolean       metaKey = false;

  unsigned long location = 0;
  boolean       repeat = false;
  boolean       isComposing = false;

  DOMString     key = "";
  DOMString     code = "";
};

dictionary KeyEventDetail : BeforeKeyEventDetail {
  boolean embeddedCancelled = false;
};

dictionary DOMWindowResizeEventDetail {
  long width = 0;
  long height = 0;
};
