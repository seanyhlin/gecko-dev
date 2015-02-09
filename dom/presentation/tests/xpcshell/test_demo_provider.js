/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

const manager = Cc['@mozilla.org/presentation-device/manager;1']
                  .getService(Ci.nsIPresentationDeviceManager);
const testProvider = Cc['@mozilla.org/presentation-device/demo-provider;1']
                       .getService(Ci.nsIPresentationDeviceProvider);

var testSenderControlChannel;
var testReceiverControlChannel;

function addProvider() {
  do_get_profile();
  Services.prefs.setCharPref('dom.presentation.demo.devices', 'test-self:127.0.0.1');
  manager.addDeviceProvider(testProvider);
  run_next_test();
}

function sessionRequest() {
  let testUrl = 'http://www.example.org/';
  let testPresentationId = 'test-presentation-id';
  let testDevice = manager.getAvailableDevices().queryElementAt(0, Ci.nsIPresentationDevice);

  Services.obs.addObserver(function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);

    let request = subject.QueryInterface(Ci.nsIPresentationSessionRequest);

    Assert.equal(request.device.id, testDevice.id, 'expected device');
    Assert.equal(request.url, testUrl, 'expected requesting URL');
    Assert.equal(request.presentationId, testPresentationId, 'expected presentation Id');

    testReceiverControlChannel = request.controlChannel;
    testReceiverControlChannel.listener = {
      onOffer: function() {},
      onAnswer: function() {},
      notifyOpened: function() {
        run_next_test();
      },
      notifyClosed: function() {},
      notifyReceiverReady: function() {},
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
    };

  }, 'presentation-session-request', false);

  // get test device
  testSenderControlChannel = testDevice.establishControlChannel(testUrl, testPresentationId);
}

function receiverReady() {
  testSenderControlChannel.listener = {
    onOffer: function() {},
    onAnswer: function() {},
    notifyOpened: function() {},
    notifyClosed: function() {},
    notifyReceiverReady: function() {
      run_next_test();
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };

  testReceiverControlChannel.receiverReady();
}

function sendOffer() {
  let testOffer = {
    type: Ci.nsIPresentationChannelDescription.TYPE_DATACHANNEL,
    dataChannelSDP: "offer",
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
  };
  testReceiverControlChannel.listener = {
    onOffer: function(offer) {
      Assert.equal(offer.type, testOffer.type, 'expected offer type');
      Assert.equal(offer.dataChannelSDP, testOffer.dataChannelSDP, 'expected offer dataChannelSDP');
      run_next_test();
    },
    onAnswer: function() {},
    notifyOpened: function() {},
    notifyClosed: function() {},
    notifyReceiverReady: function() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
  testSenderControlChannel.sendOffer(testOffer);
}

function sendAnswer() {
  let testAddrs = ['addr1', 'addr2'];
  let tcpAddresses = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  for (let address of testAddrs) {
    let wrapper = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    wrapper.data = address;
    tcpAddresses.appendElement(wrapper, false);
  }

  let testAnswer = {
    type: Ci.nsIPresentationChannelDescription.TYPE_TCP,
    tcpAddress: tcpAddresses,
    tcpPort: 5566,
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
  testSenderControlChannel.listener = {
    onOffer: function() {},
    onAnswer: function(answer) {
      Assert.equal(answer.type, testAnswer.type, 'expected answer type');
      Assert.equal(answer.tcpPort, testAnswer.tcpPort, 'expected answer tcp port');
      let addresses = answer.tcpAddress.QueryInterface(Ci.nsIArray);
      tcpAddresses = [];
      for (let idx = 0; idx < addresses.length; idx) {
        let address = addresses.queryElementAt(idx, Ci.nsISupportsString);
        tcpAddresses.push(address.data);
      }
      tcpAddresses.forEach(function(addr, idx) {
        Assert.equal(addr, testAddrs[idx], 'expected answer tcp address #'  idx);
      });
      run_next_test();
    },
    notifyOpened: function() {},
    notifyClosed: function() {},
    notifyReceiverReady: function() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
  testReceiverControlChannel.sendAnswer(testAnswer);
}

function channelClose() {
  let closeCount = 0;
  testSenderControlChannel.listener = {
    onOffer: function() {},
    onAnswer: function() {},
    notifyOpened: function() {},
    notifyClosed: function(reason) {
      Assert.equal(reason, Cr.NS_ERROR_FAILURE, 'expected reason on sender side');
      if (closeCount == 2) {
        run_next_test();
      }
    },
    notifyReceiverReady: function() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
  testReceiverControlChannel.listener = {
    onOffer: function() {},
    onAnswer: function() {},
    notifyOpened: function() {},
    notifyClosed: function(reason) {
      Assert.equal(reason, Cr.NS_OK, 'expected reason on receiver side');
      if (closeCount == 2) {
        run_next_test();
      }
    },
    notifyReceiverReady: function() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener]),
  };
  testReceiverControlChannel.close(Cr.NS_ERROR_FAILURE);
}

add_test(addProvider);
add_test(sessionRequest);
add_test(receiverReady);
add_test(sendOffer);
add_test(sendAnswer);
add_test(channelClose);

function run_test() {
  run_next_test();
}
