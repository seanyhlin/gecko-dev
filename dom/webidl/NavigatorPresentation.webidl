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
  /**
   * The requesting page use this function to start a new session, and the
   * promise is resolved to an object of PresentationSession.
   *
   * UA may show a prompt box with a list of available devices and ask the user
   * to choose one or cancel the operation.
   *
   * If |sessionId| is not assigned, a random alphanumeric value of at least 16
   * characters drawn from the characters [A-Za-z0-9] is generated for the
   * session.
   *
   * The promise may be rejected in the following cases:
   * (Internal)
   * - Service/Resource is not ready yet.
   * - Failed to send the message back to chrome process if it's invoked in a
   *   content process.
   * (External)
   * - No available device is found.
   * - Users cancel the prompt.
   * - The remote device chosen by the user is not availablie, i.e. we failed to
   *   establish a control channel between the device and us.
   * - The remote device failed to interpret the assigned url. For example, it
   *   doesn't support app protocol.
   * - For HTTP, the remote device failed to open a new window, or it failed to
   *   load the requested url.
   * - For App Protocol, the remote device failed to launch the requested
   *   application, e.g. reciver app. Note that there's a possibility that the
   *   application hasn't been installed on the device.
   *
   * The promise is kept unresolved when user ignore the prompt box.
   *
   * The promise is resolved when the remote device successfully load the url or
   * launch the requested application.
   */
  [Throws]
  Promise<PresentationSession> startSession(DOMString url, optional DOMString sessionId);

  /**
   * The requesting page use this function to reconnect an existing session, and
   * the promise is resolved to an existing PresentationSession which is created
   * by startSession() early. The following two conditions should be met:
   *   1) The session is presenting the same URL.
   *   2) Its id is the same with assigned id.
   *
   * User can use different devices to reconnect an existing session with the
   * same remote device by passing the same URL and the same id to this
   * funciton.
   *
   * No prompt is popped up.
   *
   * The promise may be rejected in the following cases:
   * (Internal)
   * - Service/Resource is not ready yet.
   * - Failed to send the message back to chrome process if it's invoked in a
   *   content process.
   * (External)
   * - No existing session which is presenting the same URL and whose id is
   *   identical to the assigned id.
   * - The remote device is not available. That is, we failed to establish a
   *   control channel with the remote device.
   * - For HTTP, the remote device failed to open a new window, or it failed to
   *   load the requested url.
   * - For App Protocol, the remote device failed to launch the requested
   *   application, e.g. reciver app. Note that there's a possibility that the
   *   application hasn't been installed on the device.
   */
  [Throws]
  Promise<PresentationSession> joinSession(DOMString url, DOMString sessionId);

  /**
   * This attribute is only available on the presenting page. It should be
   * created when loading the presenting page, and it's ready to be used after
   * 'onload' event is dispatched. 
   */
  readonly attribute PresentationSession? session;

  /**
   * It is called when session object is ready. It's a simple notification and
   * the event is fired without session object.
   */
           attribute EventHandler onsessionready;

  /**
   * Device availability. If there are more than one available devices, the
   * value is |true|. Otherwise, its value should be |false|.
   *
   * UA triggers device discovery mechanism periodically and cache the latest
   * result in this attribute. Thus, it may be out-of-date when we're not in
   * discovery mode, however, it is still useful to give the developers an idea
   * that whether there are devices nearby some time ago. 
   */
  readonly attribute boolean available;

  /**
   * It is called when device availability changes. New value is dispatched with
   * the event.
   */
           attribute EventHandler onavailablechange;
};
