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
 * The Original Code is Add-on Update Service.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gObs",
                                   "@mozilla.org/observer-service;1",
                                   "nsIObserverService");

XPCOMUtils.defineLazyServiceGetter(this, "gExtMgr",
                                   "@mozilla.org/extensions/manager;1",
                                   "nsIExtensionManager");

XPCOMUtils.defineLazyServiceGetter(this, "gIO",
                                   "@mozilla.org/network/io-service;1",
                                   "nsIIOService");

XPCOMUtils.defineLazyServiceGetter(this, "gPref",
                                   "@mozilla.org/preferences-service;1",
                                   "nsIPrefBranch2");

function getPref(func, preference, defaultValue) {
  try {
    return gPref[func](preference);
  }
  catch (e) {}
  return defaultValue;
}

// -----------------------------------------------------------------------
// Add-on auto-update management service
// -----------------------------------------------------------------------

const PREF_ADDON_UPDATE_ENABLED  = "extensions.autoupdate.enabled";
const PREF_ADDON_UPDATE_INTERVAL = "extensions.autoupdate.interval";

var gNeedsRestart = false;

function AddonUpdateService() {}

AddonUpdateService.prototype = {
  classDescription: "Add-on auto-update management",
  contractID: "@mozilla.org/browser/addon-update-service;1",
  classID: Components.ID("{93c8824c-9b87-45ae-bc90-5b82a1e4d877}"),
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback]),

  _xpcom_categories: [{ category: "update-timer",
                        value: "@mozilla.org/browser/addon-update-service;1," +
                               "getService,auto-addon-background-update-timer," +
                                PREF_ADDON_UPDATE_INTERVAL + ",86400" }],

  notify: function aus_notify(aTimer) {
    if (aTimer && !getPref("getBoolPref", PREF_ADDON_UPDATE_ENABLED, true))
      return;

    // If we already auto-upgraded and installed new versions, ignore this check
    if (gNeedsRestart)
      return;

    gIO.offline = false;

    // Extension Manager will check for empty list and auto-check all add-ons
    let items = [];
    let listener = new UpdateCheckListener();
    gExtMgr.update(items, items.length, Ci.nsIExtensionManager.UPDATE_CHECK_NEWVERSION, listener);
  }
};

// -----------------------------------------------------------------------
// Add-on update listener. Starts a download for any add-on with a viable
// update waiting
// -----------------------------------------------------------------------

function UpdateCheckListener() {
  this._addons = [];
}

UpdateCheckListener.prototype = {
  /////////////////////////////////////////////////////////////////////////////
  // nsIAddonUpdateCheckListener
  onUpdateStarted: function ucl_onUpdateStarted() {
  },

  onUpdateEnded: function ucl_onUpdateEnded() {
    if (!this._addons.length)
      return;

    // If we have some updateable add-ons, let's download them
    let items = [];
    for (let i = 0; i < this._addons.length; i++)
      items.push(gExtMgr.getItemForID(this._addons[i]));

    // Start the actual downloads
    gExtMgr.addDownloads(items, items.length, null);

    // Remember that we downloaded new updates so we don't check again
    gNeedsRestart = true;
  },

  onAddonUpdateStarted: function ucl_onAddonUpdateStarted(aAddon) {
    gObs.notifyObservers(aAddon, "addon-update-started", null);
  },

  onAddonUpdateEnded: function ucl_onAddonUpdateEnded(aAddon, aStatus) {
    gObs.notifyObservers(aAddon, "addon-update-ended", aStatus);
    
    // Save the add-on id if we can download an update
    if (aStatus == Ci.nsIAddonUpdateCheckListener.STATUS_UPDATE)
      this._addons.push(aAddon.id);
  },

  QueryInterface: function ucl_QueryInterface(aIID) {
    if (!aIID.equals(Ci.nsIAddonUpdateCheckListener) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([AddonUpdateService]);
}

