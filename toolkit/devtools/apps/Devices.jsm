/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/devtools/event-emitter.js");

const EXPORTED_SYMBOLS = ["Devices"];

let addonInstalled = false;

const Devices = {
  _devices: {},

  get helperAddonInstalled() {
    return addonInstalled;
  },
  set helperAddonInstalled(v) {
    addonInstalled = v;
    if (!addonInstalled) {
      for (let name in this._devices) {
        this.unregister(name);
      }
    }
    this.emit("addon-status-updated", v);
  },

  register: function (name, device) {
    this._devices[name] = device;
    this.emit("register");
  },

  unregister: function (name) {
    delete this._devices[name];
    this.emit("unregister");
  },

  available: function () {
    return Object.keys(this._devices).sort();
  },

  getByName: function (name) {
    return this._devices[name];
  }
};

EventEmitter.decorate(Devices);
