// -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

/** This module wraps around navigator.getBattery (https://developer.mozilla.org/en-US/docs/Web/API/Navigator.getBattery).
  * and provides a framework for spoofing battery values in test code.
  * To spoof the battery values, set `Debugging.fake = true` after exporting this with a BackstagePass,
  * after which you can spoof a property yb setting the relevant property of the BatteryManager object.
  */
this.EXPORTED_SYMBOLS = ["GetBattery", "Battery"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

// Load Services, for the BatteryManager API
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

// Values for the fake battery. See the documentation of Navigator.battery for the meaning of each field.
let gFakeBattery = {
  charging: false,
  chargingTime: 0,
  dischargingTime: Infinity,
  level: 1,
}

// BackendPass-exported object for toggling spoofing
this.Debugging = {
  /**
   * If `false`, use the DOM Battery implementation.
   * Set it to `true` if you need to fake battery values
   * for testing or debugging purposes.
   */
  fake: false
}

this.GetBattery = function () {
  return new Services.appShell.hiddenDOMWindow.Promise(function (resolve, reject) {
    // Return fake values if spoofing is enabled, otherwise fetch the real values from the BatteryManager API
    if (Debugging.fake) {
      resolve(gFakeBattery);
      return;
    }
    Services.appShell.hiddenDOMWindow.navigator.getBattery().then(resolve, reject);
  });
};

this.Battery = {};

for (let k of ["charging", "chargingTime", "dischargingTime", "level"]) {
  let prop = k;
  Object.defineProperty(this.Battery, prop, {
    get: function() {
      // Return fake value if spoofing is enabled, otherwise fetch the real value from the BatteryManager API
      if (Debugging.fake) {
        return gFakeBattery[prop];
      }
      return Services.appShell.hiddenDOMWindow.navigator.battery[prop];
    },
    set: function(fakeSetting) {
      if (!Debugging.fake) {
        throw new Error("Tried to set fake battery value when battery spoofing was disabled");
      }
      gFakeBattery[prop] = fakeSetting;
    }
  })
}
