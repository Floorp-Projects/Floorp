// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm"); /*global Services */
Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/SimpleServiceDiscovery.jsm"); /*global SimpleServiceDiscovery */

const EVENT_SERVICE_FOUND = SimpleServiceDiscovery.EVENT_SERVICE_FOUND;
const EVENT_SERVICE_LOST = SimpleServiceDiscovery.EVENT_SERVICE_LOST;

// We want to keep this page fresh while it is open, so we decrease
// our time between searches when it is opened, and revert to the
// former time between searches when it is closed.
const SEARCH_INTERVAL_IN_MILLISECONDS = 5 * 1000;

function dump(s) {
  Services.console.logStringMessage("aboutDevices :: " + s);
}

var Devices = {
  _savedSearchInterval: -1,

  init: function() {
    dump("Initializing.");
    Services.obs.addObserver(this, EVENT_SERVICE_FOUND, false);
    Services.obs.addObserver(this, EVENT_SERVICE_LOST, false);

    let button = document.getElementById("refresh");
    button.addEventListener("click", () => {
      this.updateDeviceList();
    }, false);

    let manual = document.getElementById("connect");
    manual.addEventListener("click", (evt) => {
      this.connectManually(evt);
    }, false);

    this._savedSearchInterval = SimpleServiceDiscovery.search(SEARCH_INTERVAL_IN_MILLISECONDS);

    this.updateDeviceList();
  },

  uninit: function() {
    dump("Uninitializing.");
    Services.obs.removeObserver(this, EVENT_SERVICE_FOUND);
    Services.obs.removeObserver(this, EVENT_SERVICE_LOST);

    if (this._savedSearchInterval > 0) {
      SimpleServiceDiscovery.search(this._savedSearchInterval);
    }
  },

  _createItemForDevice: function(device) {
    let item = document.createElement("div");

    let friendlyName = document.createElement("div");
    friendlyName.classList.add("name");
    friendlyName.textContent = device.friendlyName;
    item.appendChild(friendlyName);

    let location = document.createElement("div");
    location.classList.add("location");
    location.textContent = device.location;
    item.appendChild(location);

    return item;
  },

  updateDeviceList: function() {
    let services = SimpleServiceDiscovery.services;
    dump("Updating device list with " + services.length + " services.");

    let list = document.getElementById("devices-list");
    while (list.firstChild) {
      list.removeChild(list.firstChild);
    }

    for (let service of services) {
      let item = this._createItemForDevice(service);
      list.appendChild(item);
    }
  },

  observe: function(subject, topic, data) {
    if (topic == EVENT_SERVICE_FOUND || topic == EVENT_SERVICE_LOST) {
      this.updateDeviceList();
    }
  },

  _fixedTargetForType: function(type, ip) {
    let fixedTarget = {};
    if (type == "roku") {
      fixedTarget.target = "roku:ecp";
      fixedTarget.location = "http://" + ip + ":8060";
    } else if (type == "chromecast") {
      fixedTarget.target = "urn:dial-multiscreen-org:service:dial:1";
      fixedTarget.location = "http://" + ip + ":8008";
    }
    return fixedTarget;
  },

  connectManually: function(evt) {
    // Since there is no form submit event, this is not validated. However,
    // after we process this event, the element's validation state is updated.
    let ip = document.getElementById("ip");
    if (!ip.checkValidity()) {
      dump("Manually entered IP address is not valid!");
      return;
    }

    let fixedTargets = [];
    try {
      fixedTargets = JSON.parse(Services.prefs.getCharPref("browser.casting.fixedTargets"));
    } catch (e) {}

    let type = document.getElementById("type").value;
    let fixedTarget = this._fixedTargetForType(type, ip.value);

    // Early abort if we're already looking for this target.
    if (fixedTargets.indexOf(fixedTarget) > -1)
      return;

    fixedTargets.push(fixedTarget);
    Services.prefs.setCharPref("browser.casting.fixedTargets", JSON.stringify(fixedTargets));

    // The backend does not yet listen for pref changes, so we trigger a scan.
    this.updateDeviceList();
  },
};

window.addEventListener("load", Devices.init.bind(Devices), false);
window.addEventListener("unload", Devices.uninit.bind(Devices), false);
