/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/presentation-api/
 *
 * Copyright © 2014 W3C® (MIT, ERCIM, Keio, Beihang), All Rights Reserved.
 * W3C liability, trademark and document use rules apply.
 */

[Pref="dom.presentation.enabled", AvailableIn="PrivilegedApps"]
interface NavigatorPresentation : EventTarget {

  [Throws]
  Promise<PresentationSession> startSession(DOMString url, DOMString? sessionId);
  [Throws]
  Promise<PresentationSession> joinSession(DOMString url, DOMString? sessionId);

           attribute EventHandler onavailablechange;
  readonly attribute PresentationSession? session;

  readonly attribute boolean available;
};
