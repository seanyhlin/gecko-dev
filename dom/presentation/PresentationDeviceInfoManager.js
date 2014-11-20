/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

function log(msg) {
  dump("-*- PresentationDeviceInfoManager.js : " + msg + "\n");
}

const PRESENTATIONDEVICEINFOMANAGER_CID = Components.ID("{1bd66bef-f643-4be3-b690-0c656353eafd}");
const PRESENTATIONDEVICEINFOMANAGER_CONTRACTID = "@mozilla.org/presentation-device/deviceInfos;1";

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function PresentationDeviceInfoManager() {}

PresentationDeviceInfoManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classID: PRESENTATIONDEVICEINFOMANAGER_CID,
  contractID: PRESENTATIONDEVICEINFOMANAGER_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  receiveMessage: function(msg) {
    if (!msg) {
      return;
    }

    log("receive msg: " + msg.name);
    switch (msg.name) {
      case "PresentationDeviceInfoManager:OnDeviceChange": {
        let event = new this._window.Event("devicechange");
        this.__DOM_IMPL__.dispatchEvent(event);
        break;
      }
      case "PresentationDeviceInfoManager:GetAll:Result:Ok": {
        if (!msg.json) {
          return;
        }

        let resolver = this.takePromiseResolver(msg.json.requestId);

        if (!resolver) {
          return;
        }

        resolver.resolve(Cu.cloneInto(msg.json.devices, this._window));
        break;
      }
      case "PresentationDeviceInfoManager:GetAll:Result:Error": {
        if (!msg.json) {
          return;
        }

        let resolver = this.takePromiseResolver(msg.json.requestId);

        if (!resolver) {
          return;
        }

        resolver.reject(msg.json.error);
        break;
      }
    }
  },

  init: function(win) {
    log("init");
    this.initDOMRequestHelper(win, [
      {name: "PresentationDeviceInfoManager:OnDeviceChange", weakRef: true},
      {name: "PresentationDeviceInfoManager:GetAll:Result:Ok", weakRef: true},
      {name: "PresentationDeviceInfoManager:GetAll:Result:Error", weakRef: true},
    ]);
  },

  uninit: function() {
    log("uninit");
    let self = this;

    this.forEachPromiseResolver(function(k) {
      self.takePromiseResolver(k).reject("PresentationDeviceInfoManager got destroyed");
    });
  },

  get ondevicechange() {
    return this.__DOM_IMPL__.getEventHandler("ondevicechange");
  },

  set ondevicechange(handler) {
    this.__DOM_IMPL__.setEventHandler("ondevicechange", handler);
  },

  getAll: function() {
    log("getAll");
    let self = this;
    return this.createPromise(function(resolve, reject) {
      let resolverId = self.getPromiseResolverId({ resolve: resolve, reject: reject });
      cpmm.sendAsyncMessage("PresentationDeviceInfoManager:GetAll", {
        requestId: resolverId,
      });
    });
  },

  forceDiscovery: function() {
    cpmm.sendAsyncMessage("PresentationDeviceInfoManager:ForceDiscovery");
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PresentationDeviceInfoManager]);
