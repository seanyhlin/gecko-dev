/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["PresentationDeviceInfoService"];

function log(msg) {
  dump("PresentationDeviceInfoManager.jsm: " + msg + "\n");
}

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "presentationDeviceManager",
                                   "@mozilla.org/presentation-device/manager;1",
                                   "nsIPresentationDeviceManager");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

this.PresentationDeviceInfoService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIMessageListener,
                                         Ci.nsIObserver]),

  init: function() {
    log("init");
    ppmm.addWeakMessageListener("PresentationDeviceInfoManager:GetAll", this);
    ppmm.addWeakMessageListener("PresentationDeviceInfoManager:ForceDiscovery", this);
    Services.obs.addObserver(this, "presentation-device-change", true);
  },

  getAll: function(data, mm) {
    log("getAll");
    let deviceArray = presentationDeviceManager.getAvailableDevices().QueryInterface(Ci.nsIArray);
    if (!deviceArray) {
      data.error = "DataError";
      mm.sendAsyncMessage("PresentationDeviceInfoManager:GetAll:Result:Error", data);
      return;
    }

    data.devices = [];
    for (let i = 0; i < deviceArray.length; i++) {
      let device = deviceArray.queryElementAt(i, Ci.nsIPresentationDevice);
      data.devices.push({
        id: device.id,
        name: device.name,
        type: device.type,
      });
    }
    mm.sendAsyncMessage("PresentationDeviceInfoManager:GetAll:Result:Ok", data);
  },

  forceDiscovery: function() {
    log("forceDiscovery");
    presentationDeviceManager.forceDiscovery();
  },

  observe: function(subject, topic, data) {
    log("observe: " + topic);
    ppmm.broadcastAsyncMessage("PresentationDeviceInfoManager:OnDeviceChange");
  },

  receiveMessage: function(message) {
    if (!message.target.assertPermission("settings")) {
      debug("receive message " + message.name +
            " from a content process with no 'settings' privileges.");
      return null;
    }

    let msg = message.data || {};
    let mm = message.target;

    log("receiveMessage: " + message.name);
    switch (message.name) {
      case "PresentationDeviceInfoManager:GetAll": {
        this.getAll(msg, mm);
        break;
      }
      case "PresentationDeviceInfoManager:ForceDiscovery": {
        this.forceDiscovery();
        break;
      }
    }
  },
};

this.PresentationDeviceInfoService.init();
