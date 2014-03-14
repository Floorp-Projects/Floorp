/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let EXPORTED_SYMBOLS = ["Addon", "STATE_ENABLED", "STATE_DISABLED"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/addons/AddonRepository.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/addonutils.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://tps/logger.jsm");

const ADDONSGETURL = "http://127.0.0.1:4567/";
const STATE_ENABLED = 1;
const STATE_DISABLED = 2;

function GetFileAsText(file) {
  let channel = Services.io.newChannel(file, null, null);
  let inputStream = channel.open();
  if (channel instanceof Ci.nsIHttpChannel &&
      channel.responseStatus != 200) {
    return "";
  }

  let streamBuf = "";
  let sis = Cc["@mozilla.org/scriptableinputstream;1"]
            .createInstance(Ci.nsIScriptableInputStream);
  sis.init(inputStream);

  let available;
  while ((available = sis.available()) != 0) {
    streamBuf += sis.read(available);
  }

  inputStream.close();
  return streamBuf;
}

function Addon(TPS, id) {
  this.TPS = TPS;
  this.id = id;
}

Addon.prototype = {
  addon: null,

  uninstall: function uninstall() {
    // find our addon locally
    let cb = Async.makeSyncCallback();
    AddonManager.getAddonByID(this.id, cb);
    let addon = Async.waitForSyncCallback(cb);

    Logger.AssertTrue(!!addon, 'could not find addon ' + this.id + ' to uninstall');

    cb = Async.makeSpinningCallback();
    AddonUtils.uninstallAddon(addon, cb);
    cb.wait();
  },

  find: function find(state) {
    let cb = Async.makeSyncCallback();
    AddonManager.getAddonByID(this.id, cb);
    let addon = Async.waitForSyncCallback(cb);

    if (!addon) {
      Logger.logInfo("Could not find add-on with ID: " + this.id);
      return false;
    }

    this.addon = addon;

    Logger.logInfo("add-on found: " + addon.id + ", enabled: " +
                   !addon.userDisabled);
    if (state == STATE_ENABLED) {
      Logger.AssertFalse(addon.userDisabled, "add-on is disabled: " + addon.id);
      return true;
    } else if (state == STATE_DISABLED) {
      Logger.AssertTrue(addon.userDisabled, "add-on is enabled: " + addon.id);
      return true;
    } else if (state) {
      throw Error("Don't know how to handle state: " + state);
    } else {
      // No state, so just checking that it exists.
      return true;
    }
  },

  install: function install() {
    // For Install, the id parameter initially passed is really the filename
    // for the addon's install .xml; we'll read the actual id from the .xml.

    let cb = Async.makeSpinningCallback();
    AddonUtils.installAddons([{id: this.id, requireSecureURI: false}], cb);
    let result = cb.wait();

    Logger.AssertEqual(1, result.installedIDs.length, "Exactly 1 add-on was installed.");
    Logger.AssertEqual(this.id, result.installedIDs[0],
                       "Add-on was installed successfully: " + this.id);
  },

  setEnabled: function setEnabled(flag) {
    Logger.AssertTrue(this.find(), "Add-on is available.");

    let userDisabled;
    if (flag == STATE_ENABLED) {
      userDisabled = false;
    } else if (flag == STATE_DISABLED) {
      userDisabled = true;
    } else {
      throw new Error("Unknown flag to setEnabled: " + flag);
    }

    let cb = Async.makeSpinningCallback();
    AddonUtils.updateUserDisabled(this.addon, userDisabled, cb);
    cb.wait();

    return true;
  }
};
