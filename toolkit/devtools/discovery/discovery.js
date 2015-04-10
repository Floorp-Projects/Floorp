/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This implements a UDP mulitcast device discovery protocol that:
 *   * Is optimized for mobile devices
 *   * Doesn't require any special schema for service info
 *
 * To ensure it works well on mobile devices, there is no heartbeat or other
 * recurring transmission.
 *
 * Devices are typically in one of two groups: scanning for services or
 * providing services (though they may be in both groups as well).
 *
 * Scanning devices listen on UPDATE_PORT for UDP multicast traffic.  When the
 * scanning device wants to force an update of the services available, it sends
 * a status packet to SCAN_PORT.
 *
 * Service provider devices listen on SCAN_PORT for any packets from scanning
 * devices.  If one is recevied, the provider device sends a status packet
 * (listing the services it offers) to UPDATE_PORT.
 *
 * Scanning devices purge any previously known devices after REPLY_TIMEOUT ms
 * from that start of a scan if no reply is received during the most recent
 * scan.
 *
 * When a service is registered, is supplies a regular object with any details
 * about itself (a port number, for example) in a service-defined format, which
 * is then available to scanning devices.
 */

const { Cu, CC, Cc, Ci } = require("chrome");
const EventEmitter = require("devtools/toolkit/event-emitter");
const { setTimeout, clearTimeout } = require("sdk/timers");

const UDPSocket = CC("@mozilla.org/network/udp-socket;1",
                     "nsIUDPSocket",
                     "init");

const SCAN_PORT = 50624;
const UPDATE_PORT = 50625;
const ADDRESS = "224.0.0.115";
const REPLY_TIMEOUT = 5000;

const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

XPCOMUtils.defineLazyGetter(this, "converter", () => {
  let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
             createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = "utf8";
  return conv;
});

XPCOMUtils.defineLazyGetter(this, "sysInfo", () => {
  return Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
});

XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});

let logging = Services.prefs.getBoolPref("devtools.discovery.log");
function log(msg) {
  if (logging) {
    console.log("DISCOVERY: " + msg);
  }
}

/**
 * Each Transport instance owns a single UDPSocket.
 * @param port integer
 *        The port to listen on for incoming UDP multicast packets.
 */
function Transport(port) {
  EventEmitter.decorate(this);
  try {
    this.socket = new UDPSocket(port, false, Services.scriptSecurityManager.getSystemPrincipal());
    this.socket.joinMulticast(ADDRESS);
    this.socket.asyncListen(this);
  } catch(e) {
    log("Failed to start new socket: " + e);
  }
}

Transport.prototype = {

  /**
   * Send a object to some UDP port.
   * @param object object
   *        Object which is the message to send
   * @param port integer
   *        UDP port to send the message to
   */
  send: function(object, port) {
    if (logging) {
      log("Send to " + port + ":\n" + JSON.stringify(object, null, 2));
    }
    let message = JSON.stringify(object);
    let rawMessage = converter.convertToByteArray(message);
    try {
      this.socket.send(ADDRESS, port, rawMessage, rawMessage.length);
    } catch(e) {
      log("Failed to send message: " + e);
    }
  },

  destroy: function() {
    this.socket.close();
  },

  // nsIUDPSocketListener

  onPacketReceived: function(socket, message) {
    let messageData = message.data;
    let object = JSON.parse(messageData);
    object.from = message.fromAddr.address;
    let port = message.fromAddr.port;
    if (port == this.socket.port) {
      log("Ignoring looped message");
      return;
    }
    if (logging) {
      log("Recv on " + this.socket.port + ":\n" +
          JSON.stringify(object, null, 2));
    }
    this.emit("message", object);
  },

  onStopListening: function() {}

};

/**
 * Manages the local device's name.  The name can be generated in serveral
 * platform-specific ways (see |_generate|).  The aim is for each device on the
 * same local network to have a unique name.  If the Settings API is available,
 * the name is saved there to persist across reboots.
 */
function LocalDevice() {
  this._name = LocalDevice.UNKNOWN;
  if ("@mozilla.org/settingsService;1" in Cc) {
    this._settings =
      Cc["@mozilla.org/settingsService;1"].getService(Ci.nsISettingsService);
    Services.obs.addObserver(this, "mozsettings-changed", false);
  }
  this._get(); // Trigger |_get| to load name eagerly
}

LocalDevice.SETTING = "devtools.discovery.device";
LocalDevice.UNKNOWN = "unknown";

LocalDevice.prototype = {

  _get: function() {
    if (!this._settings) {
      // Without Settings API, just generate a name and stop, since the value
      // can't be persisted.
      this._generate();
      return;
    }
    // Initial read of setting value
    this._settings.createLock().get(LocalDevice.SETTING, {
      handle: (_, name) => {
        if (name && name !== LocalDevice.UNKNOWN) {
          this._name = name;
          log("Device: " + this._name);
          return;
        }
        // No existing name saved, so generate one.
        this._generate();
      },
      handleError: () => log("Failed to get device name setting")
    });
  },

  /**
   * Generate a new device name from various platform-specific properties.
   * Triggers the |name| setter to persist if needed.
   */
  _generate: function() {
    if (Services.appinfo.widgetToolkit == "gonk") {
      // For Gonk devices, create one from the device name plus a little
      // randomness.  The goal is just to distinguish devices in an office
      // environment where many people may have the same device model for
      // testing purposes (which would otherwise all report the same name).
      let name = libcutils.property_get("ro.product.device");
      // Pick a random number from [0, 2^32)
      let randomID = Math.floor(Math.random() * Math.pow(2, 32));
      // To hex and zero pad
      randomID = ("00000000" + randomID.toString(16)).slice(-8);
      this.name = name + "-" + randomID;
    } else {
      this.name = sysInfo.get("host");
    }
  },

  /**
   * Observe any changes that might be made via the Settings app
   */
  observe: function(subject, topic, data) {
    if (topic !== "mozsettings-changed") {
      return;
    }
    if ("wrappedJSObject" in subject) {
      subject = subject.wrappedJSObject;
    }
    if (subject.key !== LocalDevice.SETTING) {
      return;
    }
    this._name = subject.value;
    log("Device: " + this._name);
  },

  get name() {
    return this._name;
  },

  set name(name) {
    if (!this._settings) {
      this._name = name;
      log("Device: " + this._name);
      return;
    }
    // Persist to Settings API
    // The new value will be seen and stored by the observer above
    this._settings.createLock().set(LocalDevice.SETTING, name, {
      handle: () => {},
      handleError: () => log("Failed to set device name setting")
    });
  }

};

function Discovery() {
  EventEmitter.decorate(this);

  this.localServices = {};
  this.remoteServices = {};
  this.device = new LocalDevice();
  this.replyTimeout = REPLY_TIMEOUT;

  // Defaulted to Transport, but can be altered by tests
  this._factories = { Transport: Transport };

  this._transports = {
    scan: null,
    update: null
  };
  this._expectingReplies = {
    from: new Set()
  };

  this._onRemoteScan = this._onRemoteScan.bind(this);
  this._onRemoteUpdate = this._onRemoteUpdate.bind(this);
  this._purgeMissingDevices = this._purgeMissingDevices.bind(this);

  Services.obs.addObserver(this, "network-active-changed", false);
}

Discovery.prototype = {

  /**
   * Add a new service offered by this device.
   * @param service string
   *        Name of the service
   * @param info object
   *        Arbitrary data about the service to announce to scanning devices
   */
  addService: function(service, info) {
    log("ADDING LOCAL SERVICE");
    if (Object.keys(this.localServices).length === 0) {
      this._startListeningForScan();
    }
    this.localServices[service] = info;
  },

  /**
   * Remove a service offered by this device.
   * @param service string
   *        Name of the service
   */
  removeService: function(service) {
    delete this.localServices[service];
    if (Object.keys(this.localServices).length === 0) {
      this._stopListeningForScan();
    }
  },

  /**
   * Scan for service updates from other devices.
   */
  scan: function() {
    this._startListeningForUpdate();
    this._waitForReplies();
    // TODO Bug 1027457: Use timer to debounce
    this._sendStatusTo(SCAN_PORT);
  },

  /**
   * Get a list of all remote devices currently offering some service.:w
   */
  getRemoteDevices: function() {
    let devices = new Set();
    for (let service in this.remoteServices) {
      for (let device in this.remoteServices[service]) {
        devices.add(device);
      }
    }
    return [...devices];
  },

  /**
   * Get a list of all remote devices currently offering a particular service.
   */
  getRemoteDevicesWithService: function(service) {
    let devicesWithService = this.remoteServices[service] || {};
    return Object.keys(devicesWithService);
  },

  /**
   * Get service info (any details registered by the remote device) for a given
   * service on a device.
   */
  getRemoteService: function(service, device) {
    let devicesWithService = this.remoteServices[service] || {};
    return devicesWithService[device];
  },

  _waitForReplies: function() {
    clearTimeout(this._expectingReplies.timer);
    this._expectingReplies.from = new Set(this.getRemoteDevices());
    this._expectingReplies.timer =
      setTimeout(this._purgeMissingDevices, this.replyTimeout);
  },

  get Transport() {
    return this._factories.Transport;
  },

  _startListeningForScan: function() {
    if (this._transports.scan) {
      return; // Already listening
    }
    log("LISTEN FOR SCAN");
    this._transports.scan = new this.Transport(SCAN_PORT);
    this._transports.scan.on("message", this._onRemoteScan);
  },

  _stopListeningForScan: function() {
    if (!this._transports.scan) {
      return; // Not listening
    }
    this._transports.scan.off("message", this._onRemoteScan);
    this._transports.scan.destroy();
    this._transports.scan = null;
  },

  _startListeningForUpdate: function() {
    if (this._transports.update) {
      return; // Already listening
    }
    log("LISTEN FOR UPDATE");
    this._transports.update = new this.Transport(UPDATE_PORT);
    this._transports.update.on("message", this._onRemoteUpdate);
  },

  _stopListeningForUpdate: function() {
    if (!this._transports.update) {
      return; // Not listening
    }
    this._transports.update.off("message", this._onRemoteUpdate);
    this._transports.update.destroy();
    this._transports.update = null;
  },

  observe: function(subject, topic, data) {
    if (topic !== "network-active-changed") {
      return;
    }
    let activeNetwork = subject;
    if (!activeNetwork) {
      log("No active network");
      return;
    }
    activeNetwork = activeNetwork.QueryInterface(Ci.nsINetworkInterface);
    log("Active network changed to: " + activeNetwork.type);
    // UDP sockets go down when the device goes offline, so we'll restart them
    // when the active network goes back to WiFi.
    if (activeNetwork.type === Ci.nsINetworkInterface.NETWORK_TYPE_WIFI) {
      this._restartListening();
    }
  },

  _restartListening: function() {
    if (this._transports.scan) {
      this._stopListeningForScan();
      this._startListeningForScan();
    }
    if (this._transports.update) {
      this._stopListeningForUpdate();
      this._startListeningForUpdate();
    }
  },

  /**
   * When sending message, we can use either transport, so just pick the first
   * one currently alive.
   */
  get _outgoingTransport() {
    if (this._transports.scan) {
      return this._transports.scan;
    }
    if (this._transports.update) {
      return this._transports.update;
    }
    return null;
  },

  _sendStatusTo: function(port) {
    let status = {
      device: this.device.name,
      services: this.localServices
    };
    this._outgoingTransport.send(status, port);
  },

  _onRemoteScan: function() {
    // Send my own status in response
    log("GOT SCAN REQUEST");
    this._sendStatusTo(UPDATE_PORT);
  },

  _onRemoteUpdate: function(e, update) {
    log("GOT REMOTE UPDATE");

    let remoteDevice = update.device;
    let remoteHost = update.from;

    // Record the reply as received so it won't be purged as missing
    this._expectingReplies.from.delete(remoteDevice);

    // First, loop over the known services
    for (let service in this.remoteServices) {
      let devicesWithService = this.remoteServices[service];
      let hadServiceForDevice = !!devicesWithService[remoteDevice];
      let haveServiceForDevice = service in update.services;
      // If the remote device used to have service, but doesn't any longer, then
      // it was deleted, so we remove it here.
      if (hadServiceForDevice && !haveServiceForDevice) {
        delete devicesWithService[remoteDevice];
        log("REMOVED " + service + ", DEVICE " + remoteDevice);
        this.emit(service + "-device-removed", remoteDevice);
      }
    }

    // Second, loop over the services in the received update
    for (let service in update.services) {
      // Detect if this is a new device for this service
      let newDevice = !this.remoteServices[service] ||
                      !this.remoteServices[service][remoteDevice];

      // Look up the service info we may have received previously from the same
      // remote device
      let devicesWithService = this.remoteServices[service] || {};
      let oldDeviceInfo = devicesWithService[remoteDevice];

      // Store the service info from the remote device
      let newDeviceInfo = Cu.cloneInto(update.services[service], {});
      newDeviceInfo.host = remoteHost;
      devicesWithService[remoteDevice] = newDeviceInfo;
      this.remoteServices[service] = devicesWithService;

      // If this is a new service for the remote device, announce the addition
      if (newDevice) {
        log("ADDED " + service + ", DEVICE " + remoteDevice);
        this.emit(service + "-device-added", remoteDevice, newDeviceInfo);
      }

      // If we've seen this service from the remote device, but the details have
      // changed, announce the update
      if (!newDevice &&
          JSON.stringify(oldDeviceInfo) != JSON.stringify(newDeviceInfo)) {
        log("UPDATED " + service + ", DEVICE " + remoteDevice);
        this.emit(service + "-device-updated", remoteDevice, newDeviceInfo);
      }
    }
  },

  _purgeMissingDevices: function() {
    log("PURGING MISSING DEVICES");
    for (let service in this.remoteServices) {
      let devicesWithService = this.remoteServices[service];
      for (let remoteDevice in devicesWithService) {
        // If we're still expecting a reply from a remote device when it's time
        // to purge, then the service is removed.
        if (this._expectingReplies.from.has(remoteDevice)) {
          delete devicesWithService[remoteDevice];
          log("REMOVED " + service + ", DEVICE " + remoteDevice);
          this.emit(service + "-device-removed", remoteDevice);
        }
      }
    }
  }

};

let discovery = new Discovery();

module.exports = discovery;
