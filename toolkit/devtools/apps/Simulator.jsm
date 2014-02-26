/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/devtools/event-emitter.js");

const EXPORTED_SYMBOLS = ["Simulator"];

const Simulator = {
  _simulators: {},

  register: function (version, simulator) {
    this._simulators[version] = simulator;
    this.emit("register");
  },

  unregister: function (version) {
    delete this._simulators[version];
    this.emit("unregister");
  },

  availableVersions: function () {
    return Object.keys(this._simulators).sort();
  },

  getByVersion: function (version) {
    return this._simulators[version];
  }
};

EventEmitter.decorate(Simulator);
