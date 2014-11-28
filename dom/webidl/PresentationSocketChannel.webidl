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

enum PresentationBinaryType { "blob", "arraybuffer" };

[Pref="dom.presentation.enabled", AvailableIn="PrivilegedApps"]
interface PresentationSocketChannel : EventTarget {
  [Throws]
  void send(DOMString data);
  [Throws]
  void send(Blob data);
  [Throws]
  void send(ArrayBuffer data);
  [Throws]
  void send(ArrayBufferView data);

  void close();

           attribute PresentationBinaryType binaryType;
  readonly attribute unsigned long bufferedAmount;

  const unsigned short CONNECTING = 0;
  const unsigned short OPEN = 1;
  const unsigned short CLOSING = 2;
  const unsigned short CLOSED = 3;
  readonly attribute unsigned short readyState;

           attribute EventHandler onmessage;
           attribute EventHandler onopen;
           attribute EventHandler onerror;
           attribute EventHandler onclose;
};
