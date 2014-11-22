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

enum PresentationSessionState {
  "connected",
  "disconnected"
  /*, "resumed" */
};

enum PresentationBinaryType {
  "blob",
  "arrayBuffer"
};

enum PresentationReadyState {
  "connecting",
  "open",
  "closing",
  "closed"
};

[Pref="dom.presentation.enabled", AvailableIn="PrivilegedApps"]
interface PresentationSession : EventTarget {
  readonly attribute DOMString? id;

  readonly attribute unsigned long bufferedAmount;

  readonly attribute PresentationReadyState readyState;

  readonly attribute PresentationSessionState sessionState;

           attribute PresentationBinaryType binaryType;

           attribute EventHandler onsessionstatechange;

           attribute EventHandler onmessage;

           attribute EventHandler onopen;

           attribute EventHandler onerror;

           attribute EventHandler onclose;

  [Throws]
  void send((DOMString or Blob or ArrayBuffer or ArrayBufferView) data);

  [Throws]
  void close();

};
