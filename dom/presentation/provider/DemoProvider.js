/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PRESENTATION_SERVICE = "presentation";

XPCOMUtils.defineLazyGetter(this, "converter", () => {
  let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
             createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = "utf8";
  return conv;
});

function log(msg) {
  dump("-*- DemoProvider.js: " + msg + "\n");
}

function DeviceInfo(name, ip, port) {
  this.name = name;
  this.ip = ip;
  this.port = port;
}

const SIGNALING_PORT = 5566;

function SignalingChannel() {
  this._socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);
  this._socket.init(SIGNALING_PORT, false);
  this._socket.asyncListen(this);
}

SignalingChannel.prototype = {
  onmessage: null,

  //nsIUDPSocketListener
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIUDPSocketListener]),

  onPacketReceived : function(aSocket, aMessage){
    log('SignalingChannel - receive remote message: ' + aMessage.data);
    let device = new DeviceInfo('remote', aMessage.fromAddr.address, aMessage.fromAddr.port);
    let msg = JSON.parse(aMessage.data);
    if (this.onmessage) {
      this.onmessage(device, msg);
    }
  },

  onStopListening: function(aSocket, aStatus){
    this.onmessage = null;
  },

  send: function(device, msg) {
    log("SignalingChannel - Send to " + JSON.stringify(device) + ": " + JSON.stringify(msg, null, 2));

    let message = JSON.stringify(msg);
    let rawMessage = converter.convertToByteArray(message);
    try {
      this._socket.send(device.ip, device.port, rawMessage, rawMessage.length);
    } catch(e) {
      log("SignalingChannel - Failed to send message: " + e);
    }
  },

  close: function() {
    log("SignalingChannel - close signalling channel");
    if (this._socket) {
      this._socket.close();
      this._socket = null;
    }
  },

  get port() {
    return this._socket.port;
  },
};


function PresentationServer() {}

/*
|requestSession|
  requestSession(offer)  ->     send(reqeustSession{offer})    ==> onSessionRequest(offer)
        notifyOpened() <== send(requestSession:Answer{answer}) <- responseSession(answer)

|sessionSend|
  sessionSend(channelId, msg) -> send(ondata{channelId, msg}) ==> channel.onMessage(msg)

|sessionClose|
  sessionClose(channelId) -> send(sessionClose{channelId}) ==> channel.notifyClosed()
 */
PresentationServer.prototype = {
  _transports: {}, // presentationId -> transport
  _receivers: {}, // presentationId -> transport
  _waitForResponse: {},

  init: function() {
    log("PresentationServer - init");
    this._channel = new SignalingChannel();
    this._channel.onmessage = this._handleMessage.bind(this);
  },

  deinit: function() {
    log("PresentationServer - deinit");
    if (this._channel) {
      this._channel.close();
    }
  },

  requestSession: function(device, url, presentationId) {
    log("PresentationServer - requestSession to " + JSON.stringify(device) + ": " + url + ", " + presentationId);
    let msg = {
      type: "requestSession",
      url: url,
      presentationId: presentationId,
    };
    this._channel.send(device, msg);
    let transport = new DemoControlChannel(device, presentationId, 'sender');
    this._transports[presentationId] = transport;
    transport.established();
    return transport;
  },

  responseSession: function(device, presentationId) {
    log("PresentationServer - responseSession to " + JSON.stringify(device) + ": " + presentationId);
    let transport = new DemoControlChannel(device, presentationId, 'receiver');
    this._receivers[presentationId] = transport;
    transport.established();
    return transport;
  },

  sendOffer: function(device, presentationId, offer) {
    log("PresentationServer - sendOffer to " + JSON.stringify(device) + ": " + presentationId + ", " + JSON.stringify(offer));
    let msg = {
      type: "requestSession:Offer",
      presentationId: presentationId,
      offer: offer,
    };
    this._channel.send(device, msg);
  },

  sendAnswer: function(device, presentationId, answer) {
    log("PresentationServer - response to " + JSON.stringify(device) + ": " + presentationId + ", " + JSON.stringify(answer));
    let msg = {
      type: "requestSession:Answer",
      presentationId: presentationId,
      answer: answer,
    };
    this._channel.send(device, msg);
  },

  receiverReady: function(device, presentationId) {
    log("PresentationServer - sendOffer to " + JSON.stringify(device) + ": " + presentationId);
    let msg = {
      type: "requestSession:ReceiverReady",
      presentationId: presentationId,
    };
    this._channel.send(device, msg);
  },

  _handleMessage: function(device, msg) {
    log("PresentationServer - handleMessage from " + JSON.stringify(device) + ": " + JSON.stringify(msg));
    switch (msg.type) {
      case "requestSession": {
        let dev = null;
        for (let key in devices) {
          if (devices[key].host === device.ip) {
            dev = devices[key];
            break;
          }
        }
        if (!dev) {
          //XXX auto add device that not in device list
          log("PresentationServer - handle requestSession - create device");
          let manager = Cc["@mozilla.org/presentation-device/demo-provider;1"]
                          .getService(Ci.nsIPresentationDeviceProvider);
          dev = new DemoDevice({name: device.ip,
                                host: device.ip,
                                port: SIGNALING_PORT});
          devices[device.ip] = dev;
          manager.listener.QueryInterface(Ci.nsIPresentationDeviceListener).addDevice(dev);
        }
        dev.onRequestSession(msg.url, msg.presentationId);
        return;
        break;
      }
      case "requestSession:Offer": {
        let transport = this._receivers[msg.presentationId];
        log(JSON.stringify(Object.keys(this._receivers)));
        if (!transport) {
          log("no transport for " + msg.presentationId);
          return;
        }
        //XXX android device cannot get self ip.
				msg.offer.tcpAddresses[0] = device.ip;
        transport.onOffer(msg.offer);
        break;
      }
      case "requestSession:Answer": {
        let transport = this._transports[msg.presentationId];
        if (!transport) {
          return;
        }
        transport.onAnswer(msg.answer);
        break;
      }
      case "sessionClose": {
        let transport = (msg.direction === 'receiver') ? this._transports[msg.presentationId]
                                                       : this._receivers[msg.presentationId];
        if (!transport) {
          return;
        }
        transport.onClose(msg.reason);
        if (msg.direction === 'receiver') {
          delete this._transports[msg.presentationId];
        } else {
          delete this._receivers[msg.presentationId];
        }
        break;
      }
      case "requestSession:ReceiverReady": {
        let transport = this._transports[msg.presentationId];
        if (!transport) {
          return;
        }
        transport.onReceiverReady();
        break;
      }
    }
  },

  sessionClose: function(device, presentationId, reason, direction) {
    log('PresentationServer - sessionClose to ' + JSON.stringify(device) + ': ' + presentationId);
    let msg = {
      type: 'sessionClose',
      presentationId: presentationId,
      reason: reason,
      direction: direction,
    };
    this._channel.send(device, msg);

    let transport = (direction === 'sender') ? this._transports[presentationId]
                                             : this._receivers[presentationId];
    transport.onClose(Cr.NS_OK);
    if (direction === 'sender') {
      delete this._transports[presentationId];
    } else {
      delete this._receivers[presentationId];
    }
  },
};

var presentationServer = new PresentationServer();

function ChannelDescription(init) {
  log("create ChannelDescription: " + JSON.stringify(init));

  this._type = init.type;
  switch (this._type) {
    case Ci.nsIPresentationChannelDescription.TYPE_TCP:
      this._tcpAddresses = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      for (let address of init.tcpAddresses) {
        let wrapper = Cc["@mozilla.org/supports-cstring;1"].createInstance(Ci.nsISupportsCString);
        wrapper.data = address;
        this._tcpAddresses.appendElement(wrapper, false);
      }

      this._tcpPort = init.tcpPort;
      break;
    case Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL:
      this._dataChannelSDP = init.dataChannelSDP;
      break;
  }
}

ChannelDescription.prototype = {
  _type: 0,
  _tcpAddresses: null,
  _tcpPort: 0,
  _dataChannelSDP: "",
  get type() {
    return this._type;
  },
  get tcpAddress() {
    return this._tcpAddresses;
  },
  get tcpPort() {
    return this._tcpPort;
  },
  get dataChannelSDP() {
    return this._dataChannelSDP;
  },

  classID: Components.ID("{791ef868-d984-4e64-a38a-e15d70938921}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
};

// adapter to Presentation Device Management interfaces.
function DemoControlChannel(device, presentationId, direction) {
  log("create DemoControlChannel: " + presentationId);
  this._device = device;
  this._presentationId = presentationId;
  this._direction = direction;
}

DemoControlChannel.prototype = {
  _connected: false,
  _pendingOpen: false,
  _pendingOffer: null,
  _pendingAnswer: null,
  receiverReady: function() {
    log("DemoControlChannel - receiverReady");
    if (!this._connected) {
      return;
    }
    presentationServer.receiverReady(this._device, this._presentationId);
  },
  sendOffer: function(offer) {
    let offerInit = this._convertToJson(offer);
    log("DemoControlChannel - sendOffer: " + JSON.stringify(offerInit));
    if (!this._connected) {
      return;
    }
    presentationServer.sendOffer(this._device, this._presentationId, offerInit);
  },
  sendAnswer: function(answer) {
    let answerInit = this._convertToJson(answer);
    log("DemoControlChannel - sendAnswer: " + JSON.stringify(answerInit));
    if (!this._connected) {
      return;
    }
    presentationServer.sendAnswer(this._device, this._presentationId, answerInit);
  },
  close: function(reason) {
    log("DemoControlChannel - close");
    if (!this._connected) {
      return;
    }
    presentationServer.sessionClose(this._device, this._presentationId, reason, this._direction);
  },
  get listener() {
    return this._listener;
  },
  set listener(listener) {
    log("DemoControlChannel - set listener: " + listener);
    if (!listener) {
      this._listener = null;
      return;
    }

    this._listener = listener;
    if (this._pendingOpen) {
      this._pendingOpen = false;
      log("DemoControlChannel - notify pending opened");
      this._listener.notifyOpened();
    }

    if (this._pendingOffer) {
      let offer = this._pendingOffer;
      log("DemoControlChannel - notify pending offer: " + JSON.stringify(offer));
      this._listener.onOffer(new ChannelDescription(offer));
      this._pendingOffer = null;
    }

    if (this._pendingAnswer) {
      let answer = this._pendingAnswer;
      log("DemoControlChannel - notify pending answer: " + JSON.stringify(answer));
      this._listener.onAnswer(new ChannelDescription(answer));
      this._pendingAnswer = null;
    }
    if (this._pendingReceiverReady) {
      log("DemoControlChannel - notify pending receiverReady");
      this._listener.notifyReceiverReady();
      this._pendingReceiverReady = false;
    }

    if (this._pendingClose) {
      this._pendingClose = false;
      log("DemoControlChannel - notify pending closed");
      this._listener.notifyClosed(this._pendingCloseReason);
    }
  },

  established: function() {
    this._connected = true;
    if (!this._listener) {
      this._pendingOpen = true;
      return;
    }
    log("DemoControlChannel - notify opened");
    this._listener.notifyOpened();
  },
  onOffer: function(offer) {
		log(this._connected + ", " + this._listener);
    if (!this._connected) {
      return;
    }
    if (!this._listener) {
      this._pendingOffer = offer;
      return;
    }
    log("DemoControlChannel - notify offer: " + JSON.stringify(offer));
    this._listener.onOffer(new ChannelDescription(offer));
  },
  onAnswer: function(answer) {
    if (!this._connected) {
      return;
    }
    if (!this._listener) {
      this._pendingAnswer = answer;
      return;
    }
    log("DemoControlChannel - notify answer: " + JSON.stringify(answer));
    this._listener.onAnswer(new ChannelDescription(answer));
  },
  onClose: function(reason) {
    if (this._connected) {
      this._doClose(reason);
    }
  },
  onReceiverReady: function() {
    if (!this._connected) {
      return;
    }
    if (!this._listener) {
      this._pendingReady = true;
      return;
    }
    log("DemoControlChannel - notify receiverReady");
    this._listener.notifyReceiverReady();
  },
  _doClose: function(reason) {
    this._connected = false;
    this._pendingOpen = false;
    this._pendingOffer = null;
    this._pendingAnswer = null;
    if (!this._listener) {
      this._pendingClose = true;
      this._pendingCloseReason = reason;
      return;
    }

    log("DemoControlChannel - notify closed");
    this._listener.notifyClosed(reason);
  },
  _convertToJson: function(description) {
    let json = {};
    json.type = description.type;
    switch(description.type) {
      case Ci.nsIPresentationChannelDescription.TYPE_TCP:
        let addresses = description.tcpAddress.QueryInterface(Ci.nsIArray);
        json.tcpAddresses = [];
        for (let idx = 0; idx < addresses.length; idx++) {
          let address = addresses.queryElementAt(idx, Ci.nsISupportsCString);
          json.tcpAddresses.push(address.data);
        }
        json.tcpPort = description.tcpPort;
        break;
      case Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL:
        json.dataChannelSDP = description.dataChannelSDP;
        break;
    }
    return json;
  },

  classID: Components.ID("{6d20d71e-2558-4a0b-820a-c599f4e21ba5}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannel]),
};

function DemoDevice(info) {
  log("create DemoDevice: " + JSON.stringify(info));
  this.id = info.name;
  this.name = info.name;
  this.host = info.host;
  this.port = info.port;
}

DemoDevice.prototype = {
  type: "demo",

  establishControlChannel: function(url, presentationId) {
    log("DemoDevice - establishControlChannel: " + url + ", " + presentationId);
    return presentationServer.requestSession(this._getDeviceInfo(), url, presentationId);
  },
  get listener() {
    return this._listener;
  },
  set listener(listener) {
    log("DemoDevice - set listener");
    this._listener = listener;
  },
  onRequestSession: function(url, presentationId) {
    log("DemoDevice - onRequestSession: " + url + ", " + presentationId);
    let transport = presentationServer.responseSession(this._getDeviceInfo(), presentationId);
    this._listener.onSessionRequest(this, url, presentationId, transport);
  },

  _getDeviceInfo: function() {
    return new DeviceInfo(this.name, this.host, this.port);
  },

  classID: Components.ID("{d6492549-a4f2-4a0c-9a93-00f0e9918b0a}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevice]),
};

var devices = {};

function DemoDeviceProvider() {
  log("create DemoDeviceProvider");

  // Only B2G has nsINetworkManager
  if (Ci.nsINetworkManager) {
    Services.obs.addObserver(this, "network-active-changed", false);
  } else {
    Services.obs.addObserver(this, "network:link-status-changed", false);
  }

  Services.prefs.addObserver("dom.presentation.demo.devices", this, false);
}

DemoDeviceProvider.prototype = {
  loadDevices: function() {
    let deviceList = Services.prefs.getCharPref("dom.presentation.demo.devices");
    let devices = [];
    deviceList.trim().split(',').forEach(function(str) {
      str = str.trim();
      if (!str) {
        return;
      }

      let tokens = str.split(':');
      devices.push({
        name: tokens[0].trim(),
        host: tokens[1].trim(),
        port: SIGNALING_PORT, //Number.parseInt(tokens[2], 10);
      });
    });
    return devices;
  },

  forceDiscovery: function() {
    log("DemoDeviceProvider - forceDiscovery");
    this._removeAllDevices();
    this._addDevices(this.loadDevices());
  },

  get listener() {
    return this._listener;
  },

  set listener(listener) {
    log("DemoDeviceProvider - set listener: " + listener);
    if (listener) {
      this._listener = listener;
      this._addDevices(this.loadDevices());
      presentationServer.init();
    } else {
      this._removeAllDevices();
      this._listener = null;
      presentationServer.deinit();
    }
  },

  observe: function(subject, topic, data) {
    log("DemoDeviceProvider - observe: " + topic);
    switch (topic) {
      case "nsPref:changed": {
        this._prefChanged(subject, topic, data);
        break;
      }
      case "network-active-changed": {
        this._networkChanged(subject, topic, data);
        break;
      }
      case "network:link-status-changed": {
        this._linkStatusChanged(subject, topic, data);
        break;
      }
    }
  },

  _prefChanged: function(subject, topic, data) {
    this.forceDiscovery();
  },

  _linkStatusChanged: function(subject, topic, data) {
    // restart TCP server socket
    presentationServer.deinit();
    presentationServer.init();
  },

  _networkChanged: function(subject, topic, data) {
    if (!subject) {
      log("No active network");
      return;
    }

    let activeNetwork = subject.QueryInterface(Ci.nsINetworkInterface);
    if (activeNetwork.type === Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
      // restart TCP server socket
      presentationServer.deinit();
      presentationServer.init();
    }
  },

  _addDevices: function(deviceInfos) {
    for (let deviceInfo of deviceInfos) {
      log("DemoDeviceProvider - add device: " + JSON.stringify(deviceInfo));
      let newDevice = new DemoDevice(deviceInfo);
      devices[deviceInfo.name] = newDevice;
      this._listener.addDevice(newDevice);
    }
  },

  _removeAllDevices: function() {
    for (let device of Object.keys(devices)) {
    log("DemoDeviceProvider - remove device: " + device);
      this._listener.removeDevice(devices[device]);
    }
    devices = {};
  },

  classID: Components.ID("{ab6e00ed-42c3-47ea-98d5-2fc19f8862ec}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceProvider,
                                         Ci.nsIObserver]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DemoDeviceProvider]);
