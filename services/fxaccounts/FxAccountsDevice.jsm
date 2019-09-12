/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { log } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

const { DEVICE_TYPE_DESKTOP } = ChromeUtils.import(
  "resource://services-sync/constants.js"
);

const { PREF_ACCOUNT_ROOT } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

const PREF_LOCAL_DEVICE_NAME = PREF_ACCOUNT_ROOT + "device.name";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "pref_localDeviceName",
  PREF_LOCAL_DEVICE_NAME,
  ""
);

const PREF_DEPRECATED_DEVICE_NAME = "services.sync.client.name";

// Everything to do with FxA devices.
// TODO: Move more device stuff from FxAccounts.jsm into here - eg, device
// registration, device lists, etc.
class FxAccountsDevice {
  constructor(fxai) {
    this._fxai = fxai;
  }

  async getLocalId() {
    let data = await this._fxai.currentAccountState.getUserAccountData();
    if (!data) {
      // Without a signed-in user, there can be no device id.
      return null;
    }
    const { device } = data;
    if (await this._fxai.checkDeviceUpdateNeeded(device)) {
      return this._fxai._registerOrUpdateDevice(data);
    }
    // Return the device id that we already registered with the server.
    return device.id;
  }

  // Generate a client name if we don't have a useful one yet
  getDefaultLocalName() {
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    let user = env.get("USER") || env.get("USERNAME");
    // Note that we used to fall back to the "services.sync.username" pref here,
    // but that's no longer suitable in a world where sync might not be
    // configured. However, we almost never *actually* fell back to that, and
    // doing so sanely here would mean making this function async, which we don't
    // really want to do yet.

    // A little hack for people using the the moz-build environment on Windows
    // which sets USER to the literal "%USERNAME%" (yes, really)
    if (user == "%USERNAME%" && env.get("USERNAME")) {
      user = env.get("USERNAME");
    }

    let brand = Services.strings.createBundle(
      "chrome://branding/locale/brand.properties"
    );
    let brandName;
    try {
      brandName = brand.GetStringFromName("brandShortName");
    } catch (O_o) {
      // this only fails in tests and markh can't work out why :(
      brandName = Services.appinfo.name;
    }

    // The DNS service may fail to provide a hostname in edge-cases we don't
    // fully understand - bug 1391488.
    let hostname;
    try {
      // hostname of the system, usually assigned by the user or admin
      hostname = Cc["@mozilla.org/network/dns-service;1"].getService(
        Ci.nsIDNSService
      ).myHostName;
    } catch (ex) {
      Cu.reportError(ex);
    }
    let system =
      // 'device' is defined on unix systems
      Services.sysinfo.get("device") ||
      hostname ||
      // fall back on ua info string
      Cc["@mozilla.org/network/protocol;1?name=http"].getService(
        Ci.nsIHttpProtocolHandler
      ).oscpu;

    // It's a little unfortunate that this string is defined as being weave/sync,
    // but it's not worth moving it.
    let syncStrings = Services.strings.createBundle(
      "chrome://weave/locale/sync.properties"
    );
    return syncStrings.formatStringFromName("client.name2", [
      user,
      brandName,
      system,
    ]);
  }

  getLocalName() {
    // We used to store this in services.sync.client.name, but now store it
    // under an fxa-specific location.
    let deprecated_value = Services.prefs.getStringPref(
      PREF_DEPRECATED_DEVICE_NAME,
      ""
    );
    if (deprecated_value) {
      Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, deprecated_value);
      Services.prefs.clearUserPref(PREF_DEPRECATED_DEVICE_NAME);
    }
    let name = pref_localDeviceName;
    if (!name) {
      name = this.getDefaultLocalName();
      Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, name);
    }
    return name;
  }

  setLocalName(newName) {
    Services.prefs.clearUserPref(PREF_DEPRECATED_DEVICE_NAME);
    Services.prefs.setStringPref(PREF_LOCAL_DEVICE_NAME, newName);
    // Update the registration in the background.
    this._fxai.updateDeviceRegistration().catch(error => {
      log.warn("failed to update fxa device registration", error);
    });
  }

  getLocalType() {
    return DEVICE_TYPE_DESKTOP;
  }
}

var EXPORTED_SYMBOLS = ["FxAccountsDevice"];
