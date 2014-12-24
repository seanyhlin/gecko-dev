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
};

[Pref="dom.presentation.enabled", AvailableIn="PrivilegedApps"]
interface PresentationSession : EventTarget {
  /**
   * Unique id for all existing sessions.
   */
  readonly attribute DOMString id;

  /**
   * @value connected: Use send() to send messages and close() to terminate the
   *                   session. Listen |onmessage| to receive messages.
   * @value disconnected: No operation is allowed at this state.
   */
  readonly attribute PresentationSessionState state;

  /**
   * It is called when session state changes. New value is dispatched with
   * the event.
   */
           attribute EventHandler onstatechange;

  /**
   * This function is useful only if |state == 'connected'|.
   */
  [Throws]
  void send((DOMString or Blob or ArrayBuffer or ArrayBufferView) data);

  /**
   * This function is useful only if |state == 'connected'|.
   */
  [Throws]
   void disconnect();

  /**
   * It is called when receiving messages. 
   */
           attribute EventHandler onmessage;
};
