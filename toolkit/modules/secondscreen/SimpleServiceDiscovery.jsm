// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SimpleServiceDiscovery"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var log = Cu.reportError;
XPCOMUtils.defineLazyGlobalGetters(this, ["XMLHttpRequest"]);

XPCOMUtils.defineLazyGetter(this, "converter", function() {
  let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(
    Ci.nsIScriptableUnicodeConverter
  );
  conv.charset = "utf8";
  return conv;
});

// Spec information:
// https://tools.ietf.org/html/draft-cai-ssdp-v1-03
// http://www.dial-multiscreen.org/dial-protocol-specification
const SSDP_PORT = 1900;
const SSDP_ADDRESS = "239.255.255.250";

const SSDP_DISCOVER_PACKET =
  "M-SEARCH * HTTP/1.1\r\n" +
  "HOST: " +
  SSDP_ADDRESS +
  ":" +
  SSDP_PORT +
  "\r\n" +
  'MAN: "ssdp:discover"\r\n' +
  "MX: 2\r\n" +
  "ST: %SEARCH_TARGET%\r\n\r\n";

const SSDP_DISCOVER_ATTEMPTS = 3;
const SSDP_DISCOVER_DELAY = 500;
const SSDP_DISCOVER_TIMEOUT_MULTIPLIER = 2;
const SSDP_TRANSMISSION_INTERVAL = 1000;

const EVENT_SERVICE_FOUND = "ssdp-service-found";
const EVENT_SERVICE_LOST = "ssdp-service-lost";

/*
 * SimpleServiceDiscovery manages any discovered SSDP services. It uses a UDP
 * broadcast to locate available services on the local network.
 */
var SimpleServiceDiscovery = {
  get EVENT_SERVICE_FOUND() {
    return EVENT_SERVICE_FOUND;
  },
  get EVENT_SERVICE_LOST() {
    return EVENT_SERVICE_LOST;
  },

  _devices: new Map(),
  _services: new Map(),
  _searchSocket: null,
  _searchInterval: 0,
  _searchTimestamp: 0,
  _searchTimeout: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),
  _searchRepeat: Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer),
  _discoveryMethods: [],

  _forceTrailingSlash(aURL) {
    // Cleanup the URL to make it consistent across devices
    try {
      aURL = Services.io.newURI(aURL).spec;
    } catch (e) {}
    return aURL;
  },

  // nsIUDPSocketListener implementation
  onPacketReceived(aSocket, aMessage) {
    // Listen for responses from specific devices. There could be more than one
    // available.
    let response = aMessage.data.split("\n");
    let service = {};
    response.forEach(function(row) {
      let name = row.toUpperCase();
      if (name.startsWith("LOCATION")) {
        service.location = row.substr(10).trim();
      } else if (name.startsWith("ST")) {
        service.target = row.substr(4).trim();
      }
    });

    if (service.location && service.target) {
      service.location = this._forceTrailingSlash(service.location);

      // When we find a valid response, package up the service information
      // and pass it on.
      try {
        this._processService(service);
      } catch (e) {}
    }
  },

  onStopListening(aSocket, aStatus) {
    // This is fired when the socket is closed expectedly or unexpectedly.
    // nsITimer.cancel() is a no-op if the timer is not active.
    this._searchTimeout.cancel();
    this._searchSocket = null;
  },

  // Start a search. Make it continuous by passing an interval (in milliseconds).
  // This will stop a current search loop because the timer resets itself.
  // Returns the existing search interval.
  search: function search(aInterval) {
    let existingSearchInterval = this._searchInterval;
    if (aInterval > 0) {
      this._searchInterval = aInterval || 0;
      this._searchRepeat.initWithCallback(
        this._search.bind(this),
        this._searchInterval,
        Ci.nsITimer.TYPE_REPEATING_SLACK
      );
    }
    this._search();
    return existingSearchInterval;
  },

  // Stop the current continuous search
  stopSearch: function stopSearch() {
    this._searchRepeat.cancel();
  },

  _usingLAN() {
    let network = Cc["@mozilla.org/network/network-link-service;1"].getService(
      Ci.nsINetworkLinkService
    );
    return (
      network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_WIFI ||
      network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_ETHERNET ||
      network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN
    );
  },

  _search: function _search() {
    // If a search is already active, shut it down.
    this._searchShutdown();

    // We only search if on local network
    if (!this._usingLAN()) {
      return;
    }

    // Update the timestamp so we can use it to clean out stale services the
    // next time we search.
    this._searchTimestamp = Date.now();

    // Look for any fixed IP devices. Some routers might be configured to block
    // UDP broadcasts, so this is a way to skip discovery.
    this._searchFixedDevices();

    // Look for any devices via registered external discovery mechanism.
    this._startExternalDiscovery();

    // Perform a UDP broadcast to search for SSDP devices
    let socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(
      Ci.nsIUDPSocket
    );
    try {
      socket.init(
        SSDP_PORT,
        false,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
      socket.joinMulticast(SSDP_ADDRESS);
      socket.asyncListen(this);
    } catch (e) {
      // We were unable to create the broadcast socket. Just return, but don't
      // kill the interval timer. This might work next time.
      log("failed to start socket: " + e);
      return;
    }

    // Make the timeout SSDP_DISCOVER_TIMEOUT_MULTIPLIER times as long as the time needed to send out the discovery packets.
    const SSDP_DISCOVER_TIMEOUT =
      this._devices.size *
      SSDP_DISCOVER_ATTEMPTS *
      SSDP_TRANSMISSION_INTERVAL *
      SSDP_DISCOVER_TIMEOUT_MULTIPLIER;
    this._searchSocket = socket;
    this._searchTimeout.initWithCallback(
      this._searchShutdown.bind(this),
      SSDP_DISCOVER_TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

    let data = SSDP_DISCOVER_PACKET;

    // Send discovery packets out at 1 per SSDP_TRANSMISSION_INTERVAL and send each SSDP_DISCOVER_ATTEMPTS times
    // to allow for packet loss on noisy networks.
    let timeout = SSDP_DISCOVER_DELAY;
    for (let attempts = 0; attempts < SSDP_DISCOVER_ATTEMPTS; attempts++) {
      for (let [, /* key */ device] of this._devices) {
        let target = device.target;
        setTimeout(function() {
          let msgData = data.replace("%SEARCH_TARGET%", target);
          try {
            let msgRaw = converter.convertToByteArray(msgData);
            socket.send(SSDP_ADDRESS, SSDP_PORT, msgRaw);
          } catch (e) {
            log("failed to convert to byte array: " + e);
          }
        }, timeout);
        timeout += SSDP_TRANSMISSION_INTERVAL;
      }
    }
  },

  _searchFixedDevices: function _searchFixedDevices() {
    let fixedDevices = Services.prefs.getCharPref(
      "browser.casting.fixedDevices",
      ""
    );

    if (!fixedDevices) {
      return;
    }

    fixedDevices = JSON.parse(fixedDevices);
    for (let fixedDevice of fixedDevices) {
      // Verify we have the right data
      if (!("location" in fixedDevice) || !("target" in fixedDevice)) {
        continue;
      }

      fixedDevice.location = this._forceTrailingSlash(fixedDevice.location);

      let service = {
        location: fixedDevice.location,
        target: fixedDevice.target,
      };

      // We don't assume the fixed target is ready. We still need to ping it.
      try {
        this._processService(service);
      } catch (e) {}
    }
  },

  // Called when the search timeout is hit. We use it to cleanup the socket and
  // perform some post-processing on the services list.
  _searchShutdown: function _searchShutdown() {
    if (this._searchSocket) {
      // This will call onStopListening.
      this._searchSocket.close();

      // Clean out any stale services
      for (let [, /* key */ service] of this._services) {
        if (service.lastPing != this._searchTimestamp) {
          this.removeService(service.uuid);
        }
      }
    }

    this._stopExternalDiscovery();
  },

  getSupportedExtensions() {
    let extensions = [];
    this.services.forEach(function(service) {
      extensions = extensions.concat(service.extensions);
    }, this);
    return extensions.filter(function(extension, pos) {
      return extensions.indexOf(extension) == pos;
    });
  },

  getSupportedMimeTypes() {
    let types = [];
    this.services.forEach(function(service) {
      types = types.concat(service.types);
    }, this);
    return types.filter(function(type, pos) {
      return types.indexOf(type) == pos;
    });
  },

  registerDevice: function registerDevice(aDevice) {
    // We must have "id", "target" and "factory" defined
    if (
      !("id" in aDevice) ||
      !("target" in aDevice) ||
      !("factory" in aDevice)
    ) {
      // Fatal for registration
      throw new Error("Registration requires an id, a target and a location");
    }

    // Only add if we don't already know about this device
    if (!this._devices.has(aDevice.id)) {
      this._devices.set(aDevice.id, aDevice);
    } else {
      log("device was already registered: " + aDevice.id);
    }
  },

  unregisterDevice: function unregisterDevice(aDevice) {
    // We must have "id", "target" and "factory" defined
    if (
      !("id" in aDevice) ||
      !("target" in aDevice) ||
      !("factory" in aDevice)
    ) {
      return;
    }

    // Only remove if we know about this device
    if (this._devices.has(aDevice.id)) {
      this._devices.delete(aDevice.id);
    } else {
      log("device was not registered: " + aDevice.id);
    }
  },

  findAppForService: function findAppForService(aService) {
    if (!aService || !aService.deviceID) {
      return null;
    }

    // Find the registration for the device
    if (this._devices.has(aService.deviceID)) {
      return this._devices.get(aService.deviceID).factory(aService);
    }
    return null;
  },

  findServiceForID: function findServiceForID(aUUID) {
    if (this._services.has(aUUID)) {
      return this._services.get(aUUID);
    }
    return null;
  },

  // Returns an array copy of the active services
  get services() {
    let array = [];
    for (let [, /* key */ service] of this._services) {
      let target = this._devices.get(service.deviceID);
      service.extensions = target.extensions;
      service.types = target.types;
      array.push(service);
    }
    return array;
  },

  // Returns false if the service does not match the device's filters
  _filterService: function _filterService(aService) {
    // Loop over all the devices, looking for one that matches the service
    for (let [, /* key */ device] of this._devices) {
      // First level of match is on the target itself
      if (device.target != aService.target) {
        continue;
      }

      // If we have no filter, everything passes
      if (!("filters" in device)) {
        aService.deviceID = device.id;
        return true;
      }

      // If all the filters pass, we have a match
      let failed = false;
      let filters = device.filters;
      for (let filter in filters) {
        if (filter in aService && aService[filter] != filters[filter]) {
          failed = true;
        }
      }

      // We found a match, so link the service to the device
      if (!failed) {
        aService.deviceID = device.id;
        return true;
      }
    }

    // We didn't find any matches
    return false;
  },

  _processService: function _processService(aService) {
    // Use the REST api to request more information about this service
    let xhr = new XMLHttpRequest();
    xhr.open("GET", aService.location, true);
    xhr.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    xhr.overrideMimeType("text/xml");

    xhr.addEventListener("load", () => {
      if (xhr.status == 200) {
        let doc = xhr.responseXML;
        aService.appsURL = xhr.getResponseHeader("Application-URL");
        if (aService.appsURL && !aService.appsURL.endsWith("/")) {
          aService.appsURL += "/";
        }
        aService.friendlyName = doc.querySelector("friendlyName").textContent;
        aService.uuid = doc.querySelector("UDN").textContent;
        aService.manufacturer = doc.querySelector("manufacturer").textContent;
        aService.modelName = doc.querySelector("modelName").textContent;

        this.addService(aService);
      }
    });

    xhr.send(null);
  },

  // Add a service to the WeakMap, even if one already exists with this id.
  // Returns true if this succeeded or false if it failed
  _addService(service) {
    // Filter out services that do not match the device filter
    if (!this._filterService(service)) {
      return false;
    }

    let device = this._devices.get(service.target);
    if (device && device.mirror) {
      service.mirror = true;
    }
    this._services.set(service.uuid, service);
    return true;
  },

  addService(service) {
    // Only add and notify if we don't already know about this service
    if (!this._services.has(service.uuid)) {
      if (!this._addService(service)) {
        return;
      }
      Services.obs.notifyObservers(null, EVENT_SERVICE_FOUND, service.uuid);
    }

    // Make sure we remember this service is not stale
    this._services.get(service.uuid).lastPing = this._searchTimestamp;
  },

  removeService(uuid) {
    Services.obs.notifyObservers(null, EVENT_SERVICE_LOST, uuid);
    this._services.delete(uuid);
  },

  updateService(service) {
    if (!this._addService(service)) {
      return;
    }

    // Make sure we remember this service is not stale
    this._services.get(service.uuid).lastPing = this._searchTimestamp;
  },

  addExternalDiscovery(discovery) {
    this._discoveryMethods.push(discovery);
  },

  _startExternalDiscovery() {
    for (let discovery of this._discoveryMethods) {
      discovery.startDiscovery();
    }
  },

  _stopExternalDiscovery() {
    for (let discovery of this._discoveryMethods) {
      discovery.stopDiscovery();
    }
  },
};
