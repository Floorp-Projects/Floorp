/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Crossweave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
"use strict";

let EXPORTED_SYMBOLS = ["Addon", "STATE_ENABLED", "STATE_DISABLED"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/AddonRepository.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://tps/logger.jsm");

const ADDONSGETURL = 'http://127.0.0.1:4567/';
const STATE_ENABLED = 1;
const STATE_DISABLED = 2;

function GetFileAsText(file)
{
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
    let store = Engines.get("addons")._store;
    store.uninstallAddon(addon, cb);
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
    // We call the store's APIs for installation because it is simpler. If that
    // API is broken, it should ideally be caught by an xpcshell test. But, if
    // TPS tests fail, it's all the same: a genuite reported error.
    let store = Engines.get("addons")._store;
    store.installAddons([{id: this.id}], cb);
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

    let store = Engines.get("addons")._store;
    let cb = Async.makeSpinningCallback();
    store.updateUserDisabled(this.addon, userDisabled, cb);
    cb.wait();

    return true;
  }
};
