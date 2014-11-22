/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

addMessageListener('Presenter:SessionReqParams', function setupParams(message) {
  removeMessageListener('Presenter:SessionReqParams', setupParams);
  presentation = content.navigator.presentation;

  let url = message.u;
  let sessionId = message.s;
  let deviceId = message.d;

  var evt = document.createEvent("CustomEvent");
  evt.initCustomEvent("presentation-request-params",
                      true,
                      true,
                      [url, sessionId, deviceId]);

  var ret = presentation.dispatchEvent(evt);
});

addEventListener('DOMContentLoaded', function() {
sendAsyncMessage('Presenter-loaded');
});
