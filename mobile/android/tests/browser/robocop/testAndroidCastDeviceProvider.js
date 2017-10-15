/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Components */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// event name
const TOPIC_ANDROID_CAST_DEVICE_ADDED   = "AndroidCastDevice:Added";
const TOPIC_ANDROID_CAST_DEVICE_REMOVED = "AndroidCastDevice:Removed";
const TOPIC_ANDROID_CAST_DEVICE_START   = "AndroidCastDevice:Start";
const TOPIC_PRESENTATION_VIEW_READY     = "presentation-view-ready";

// contract ID
const ANDROID_BRIDGE_CONTRACT_ID  = "@mozilla.org/android/bridge;1";
const DEVICE_PROVIDER_CONTRACT_ID = "@mozilla.org/presentation-device/android-cast-device-provider;1";

// description info
const OFFER_ADDRESS = "192.168.123.123";
const OFFER_PORT = 123;
const ANSWER_ADDRESS = "192.168.321.321";
const ANSWER_PORT = 321;

// presentation
const PRESENTATION_ID  = "test-presentation-id";
const PRESENTATION_URL = "http://example.com";

function log(str) {
  dump("testAndroidCastDeviceProvider: " + str);
}

function TestDescription(aType, aTcpAddress, aTcpPort) {
  this.type = aType;
  this.tcpAddress = Cc["@mozilla.org/array;1"]
    .createInstance(Ci.nsIMutableArray);
  for (let address of aTcpAddress) {
    let wrapper = Cc["@mozilla.org/supports-cstring;1"]
      .createInstance(Ci.nsISupportsCString);
    wrapper.data = address;
    this.tcpAddress.appendElement(wrapper);
  }
  this.tcpPort = aTcpPort;
}

TestDescription.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationChannelDescription]),
};

function TestControlChannelListener(aRole) {
  log("TestControlChannelListener of " + aRole + " is created.");
  this._role = aRole;
  this.isNotifyConnectedCalled = new Promise((aResolve) => {
    this._isNotifyConnectedCalledResolve = aResolve;
  });
  this.isOnOfferCalled = new Promise((aResolve) => {
    this._isOnOfferCalledResolve = aResolve;
  });
  this.isOnAnswerCalled = new Promise((aResolve) => {
    this._isOnAnswerCalledResolve = aResolve;
  });
  this.isOnIceCandidateCalled = new Promise((aResolve) => {
    this._isOnIceCandidateCalledResolve = aResolve;
  });
  this.isNotifyDisconnectedCalled = new Promise((aResolve) => {
    this._isNotifyDisconnectedCalledResolve = aResolve;
  });
}

TestControlChannelListener.prototype = {
  _role: null, // used to debug
  _isNotifyConnectedCalledResolve: null,
  _isOnOfferCalledResolve: null,
  _isOnAnswerCalledResolve: null,
  _isOnIceCandidateCalledResolve: null,
  _isNotifyDisconnectedCalledResolve: null,
  isNotifyConnectedCalled: null,
  isOnOfferCalled: null,
  isOnAnswerCalled: null,
  isOnIceCandidateCalled: null,
  isNotifyDisconnectedCalled: null,
  notifyConnected: function() { this._isNotifyConnectedCalledResolve(); },
  notifyDisconnected: function(aReason) { log(this._role + " call disconnect"); this._isNotifyDisconnectedCalledResolve(); },
  onOffer: function(aOffer) { this._isOnOfferCalledResolve(); },
  onAnswer: function(aAnswer) { this._isOnAnswerCalledResolve(); },
  onIceCandidate: function(aCandidate) { this._isOnIceCandidateCalledResolve(); },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationControlChannelListener])
};

function deviceManagement() {
  do_test_pending();

  let provider = Cc[DEVICE_PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    devices: {},
    _isAddDeviceCalledResolve: null,
    _isUpdateDeviceCalledResolve: null,
    _isRemoveDeviceCalledResolve: null,
    isAddDeviceCalled: null,
    isUpdateDeviceCalled: null,
    isRemoveDeviceCalled: null,
    addDevice: function(aDevice) {
      this.devices[aDevice.id] = aDevice;
      this._isAddDeviceCalledResolve();
    },
    updateDevice: function(aDevice) {
      this.devices[aDevice.id] = aDevice;
      this._isUpdateDeviceCalledResolve();
    },
    removeDevice: function(aDevice) {
      delete this.devices[aDevice.id];
      this._isRemoveDeviceCalledResolve();
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    count: function() {
      let cnt = 0;
      for (let key in this.devices) {
        if (this.devices.hasOwnProperty(key)) {
          ++cnt;
        }
      }
      return cnt;
    },
    reset: function() {
      this._isAddDeviceCalledResolve = null;
      this._isUpdateDeviceCalledResolve = null;
      this._isRemoveDeviceCalledResolve = null;

      this.isAddDeviceCalled = new Promise((aResolve) => {
        this._isAddDeviceCalledResolve = aResolve;
      });
      this.isUpdateDeviceCalled = new Promise((aResolve) => {
        this._isUpdateDeviceCalledResolve = aResolve;
      });
      this.isRemoveDeviceCalled = new Promise((aResolve) => {
        this._isRemoveDeviceCalledResolve = aResolve;
      });
    }
  };
  listener.reset();
  // Should be no device.
  ok(listener.count() == 0, "There should be no any device in device manager.");

  // Set listener to device provider.
  provider.listener = listener;
  let device = {
    uuid: "chromecast",
    friendlyName: "chromecast"
  };

  // Sync device from Android.
  Promise.race([listener.isAddDeviceCalled, listener.isUpdateDeviceCalled])
    .then(() => {
      listener.reset();
      ok(listener.count() == 1, "There should be one device in device manager after sync device.");
      // Remove the device.
      EventDispatcher.instance.dispatch(TOPIC_ANDROID_CAST_DEVICE_REMOVED, {id: "existed-chromecast"});
      return listener.isRemoveDeviceCalled;
  }).then(() => {
      listener.reset();
      ok(listener.count() == 0, "There should be no any device after the device is removed.");
      // Add the device.
      EventDispatcher.instance.dispatch(TOPIC_ANDROID_CAST_DEVICE_ADDED, device);
      return listener.isAddDeviceCalled;
  }).then(() => {
      listener.reset();
      ok(listener.count() == 1, "There should be only one device in device manager.");
      // Add the same device, and it should trigger updateDevice.
      EventDispatcher.instance.dispatch(TOPIC_ANDROID_CAST_DEVICE_ADDED, device);
      return listener.isUpdateDeviceCalled;
  }).then(() => {
      listener.reset();
      ok(listener.count() == 1, "There should still only one device in device manager.");
      // Remove the device.
      EventDispatcher.instance.dispatch(TOPIC_ANDROID_CAST_DEVICE_REMOVED, {id: device.uuid});
      return listener.isRemoveDeviceCalled;
  }).then(() => {
      listener.reset();
      ok(listener.count() == 0, "There should be no any device after the device is removed.");
      do_test_finished();
      run_next_test();
  });
}

function presentationLaunchAndTerminate() {
  do_test_pending();

  let controllerControlChannel;
  let receiverControlChannel;
  let controllerControlChannelListener = new TestControlChannelListener("controller");
  let receiverControlChannelListener = new TestControlChannelListener("receiver");

  let provider = Cc[DEVICE_PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    devices: {},
    addDevice: function(aDevice) { this.devices[aDevice.id] = aDevice; },
    updateDevice: function(aDevice) { this.devices[aDevice.id] = aDevice; },
    removeDevice: function(aDevice) { delete this.devices[aDevice.id]; },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener,
                                           Ci.nsISupportsWeakReference]),
    onSessionRequest: function(aDeviceId, aUrl, aPresentationId, aControlChannel) {
      receiverControlChannel = aControlChannel;
      receiverControlChannel.listener = receiverControlChannelListener;
    },
    onTerminateRequest: function(aDevice, aPresentationId, aControlChannel, aIsFromReceiver) {
      receiverControlChannel = aControlChannel;
      receiverControlChannel.listener = receiverControlChannelListener;
    },
    getDevice: function(aDeviceId) { return this.devices[aDeviceId]; }
  };
  provider.listener = listener;

  let device = {
    uuid: "chromecast",
    friendlyName: "chromecast"
  };

  // Add and get the device.
  EventDispatcher.instance.dispatch(TOPIC_ANDROID_CAST_DEVICE_ADDED, device);
  let presentationDevice = listener.getDevice(device.uuid).QueryInterface(Ci.nsIPresentationDevice);
  ok(presentationDevice != null, "It should have nsIPresentationDevice interface.");

  controllerControlChannel = presentationDevice.establishControlChannel();
  controllerControlChannel.QueryInterface(Ci.nsIPresentationControlChannel);

  controllerControlChannelListener = new TestControlChannelListener("controller");
  controllerControlChannel.listener = controllerControlChannelListener;
  // test notifyConnected for controller
  controllerControlChannelListener.isNotifyConnectedCalled
    .then(() => {
      ok(true, "notifyConnected of controller should be called.");

      // test notifyConnected for receiver
      controllerControlChannel.launch(PRESENTATION_ID, PRESENTATION_URL);
      return receiverControlChannelListener.isNotifyConnectedCalled;
  }).then(() => {
      ok(true, "notifyConnected of receiver should be called.");

      // Test onOffer for receiver.
      try {
        let tcpType = Ci.nsIPresentationChannelDescription.TYPE_TCP;
        let offer = new TestDescription(Ci.nsIPresentationChannelDescription.TYPE_TCP,
                                        [OFFER_ADDRESS], OFFER_PORT);
        controllerControlChannel.sendOffer(offer);
      } catch (e) {
        ok(false, "sending offer fails:" + e);
      }
      return receiverControlChannelListener.isOnOfferCalled;
  }).then(() => {
      ok(true, "onOffer of receiver should be called.");

      // Test onAnswer for controller
      try {
        let tcpType = Ci.nsIPresentationChannelDescription.TYPE_TCP;
        let answer = new TestDescription(Ci.nsIPresentationChannelDescription.TYPE_TCP,
                                         [ANSWER_ADDRESS], ANSWER_PORT);
        receiverControlChannel.sendAnswer(answer);
      } catch (e) {
        ok(false, "sending answer fails:" + e);
      }
      return controllerControlChannelListener.isOnAnswerCalled;
  }).then(() => {
      ok(true, "onAnswer of controller should be called.");

      // Test onIceCandidate
      let candidate = {
        candidate: "1 1 UDP 1 127.0.0.1 34567 type host",
        sdpMid: "helloworld",
        sdpMLineIndex: 1
      };
      try {
        controllerControlChannel.sendIceCandidate(JSON.stringify(candidate));
      } catch (e) {
        ok(false, "sending ICE candidate fails:" + e);
      }
      return receiverControlChannelListener.isOnIceCandidateCalled;
  }).then(() => {
      ok(true, "onIceCandidate of receiver should be called.");

      // Test notifyDisconnected
      controllerControlChannel.disconnect(Cr.NS_OK);
      return Promise.all([controllerControlChannelListener.isNotifyDisconnectedCalled,
                          receiverControlChannelListener.isNotifyDisconnectedCalled]);
  }).then(() => {
      ok(true, "isNotifyDisconnectedCalled of both controller and receiver should be called.");

      // Test terminate
      controllerControlChannel = presentationDevice.establishControlChannel();
      controllerControlChannel.QueryInterface(Ci.nsIPresentationControlChannel);

      controllerControlChannelListener = new TestControlChannelListener("controller");
      controllerControlChannel.listener = controllerControlChannelListener;
      // test notifyConnected for controller
      return controllerControlChannelListener.isNotifyConnectedCalled;
  }).then(() => {
      ok(true, "notifyConnected of controller should be called.");

      // call terminate
      controllerControlChannel.terminate(PRESENTATION_ID);
      return receiverControlChannelListener.isNotifyConnectedCalled;
  }).then(() => {
      ok(true, "notifyConnected of receiver should be called.");
      do_test_finished();
      run_next_test();
  });
}

add_test(deviceManagement);
add_test(presentationLaunchAndTerminate);

run_next_test();
