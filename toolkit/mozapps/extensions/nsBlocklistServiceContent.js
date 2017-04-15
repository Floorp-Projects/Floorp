/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const kMissingAPIMessage = "Unsupported blocklist call in the child process."

/*
 * A lightweight blocklist proxy for the content process that traps plugin
 * related blocklist checks and forwards them to the parent. This interface is
 * primarily designed to insure overlays work.. it does not control plugin
 * or addon loading.
 */

function Blocklist() {
  this.init();
}

Blocklist.prototype = {
  classID: Components.ID("{e0a106ed-6ad4-47a4-b6af-2f1c8aa4712d}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIBlocklistService]),

  init() {
    Services.cpmm.addMessageListener("Blocklist:blocklistInvalidated", this);
    Services.obs.addObserver(this, "xpcom-shutdown");
  },

  uninit() {
    Services.cpmm.removeMessageListener("Blocklist:blocklistInvalidated", this);
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "xpcom-shutdown":
      this.uninit();
      break;
    }
  },

  // Message manager message handlers
  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "Blocklist:blocklistInvalidated":
        Services.obs.notifyObservers(null, "blocklist-updated");
        Services.cpmm.sendAsyncMessage("Blocklist:content-blocklist-updated");
        break;
      default:
        throw new Error("Unknown blocklist message received from content: " + aMsg.name);
    }
  },

  /*
   * A helper that queries key data from a plugin or addon object
   * and generates a simple data wrapper suitable for ipc. We hand
   * these directly to the nsBlockListService in the parent which
   * doesn't query for much.. allowing us to get away with this.
   */
  flattenObject(aTag) {
    // Based on debugging the nsBlocklistService, these are the props the
    // parent side will check on our objects.
    let props = ["name", "description", "filename", "version"];
    let dataWrapper = {};
    for (let prop of props) {
      dataWrapper[prop] = aTag[prop];
    }
    return dataWrapper;
  },

  // We support the addon methods here for completeness, but content currently
  // only calls getPluginBlocklistState.

  isAddonBlocklisted(aAddon, aAppVersion, aToolkitVersion) {
    return true;
  },

  getAddonBlocklistState(aAddon, aAppVersion, aToolkitVersion) {
    return Components.interfaces.nsIBlocklistService.STATE_BLOCKED;
  },

  // There are a few callers in layout that rely on this.
  getPluginBlocklistState(aPluginTag, aAppVersion, aToolkitVersion) {
    return Services.cpmm.sendSyncMessage("Blocklist:getPluginBlocklistState", {
      addonData: this.flattenObject(aPluginTag),
      appVersion: aAppVersion,
      toolkitVersion: aToolkitVersion
    })[0];
  },

  getAddonBlocklistURL(aAddon, aAppVersion, aToolkitVersion) {
    throw new Error(kMissingAPIMessage);
  },

  getPluginBlocklistURL(aPluginTag) {
    throw new Error(kMissingAPIMessage);
  },

  getPluginInfoURL(aPluginTag) {
    throw new Error(kMissingAPIMessage);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Blocklist]);
