/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kPresentationPresenterScript =
  "chrome://b2g/content/presentation-presenter.js";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
var pm = Cc["@mozilla.org/dom/presentationmanager;1"].getService();

function log(msg) {
  dump('presentation-session.js: ' + msg + '\n');
}

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

function onSessionRequest(url, sessionId, deviceId) {
  log(' SessionRequest on presenter - url :' + url);

  function presenterLaunchResult(evt) {
    let detail = evt.detail;
    if (detail.type !== 'presentation-launch-result') {
      return;
    }
    log('mozContentEvent: presentation-launch-result');
    SystemAppProxy.removeEventListener('mozContentEvent', presenterLaunchResult);
    let frameLoader = detail.frame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
    let mm = frameLoader.messageManager;
    if (detail.reuse) {
      log('reuse iframe');
      mm.sendAsyncMessage('Presenter:SessionReqParams', {u:url,
                                                         s:sessionId,
                                                         d:deviceId});
      return;
    }

    try {
      mm.loadFrameScript(kPresentationPresenterScript, true, true);
      mm.addMessageListener('Presenter-loaded', function presenterLoaded(msg) {
        mm.removeMessageListener("Presenter-loaded", presenterLoaded);
        mm.sendAsyncMessage('Presenter:SessionReqParams', {u:url,
                                                           s:sessionId,
                                                           d:deviceId});

      })
    } catch (e) {
      log('Error loading  as frame script: ' + e);
    }
  }

  SystemAppProxy.addEventListener('mozContentEvent', presenterLaunchResult);

  let detail = {
    type: 'presentation-launch',
    url: url,
  };
  SystemAppProxy.dispatchEvent(detail);
}

Services.obs.addObserver(function (subject, topic, data) {
  let argsArray = subject.QueryInterface(Ci.nsISupportsArray);
  if (argsArray.length == 3) {
    let url = argsArray.queryElementAt(0, Ci.nsISupportsCString).data;
    let sessionId = argsArray.queryElementAt(1, Ci.nsISupportsCString).data;
    let deviceId = argsArray.queryElementAt(2, Ci.nsISupportsCString).data;
    onSessionRequest(url, sessionId, deviceId);
  } else {
    log(' session request failed - wrong parameters !');
  }
}, 'presentation-request-from-manager', false);
