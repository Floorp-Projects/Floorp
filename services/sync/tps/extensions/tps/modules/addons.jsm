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

var EXPORTED_SYMBOLS = ["Addon", "STATE_ENABLED", "STATE_DISABLED"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://gre/modules/AddonManager.jsm");
CU.import("resource://gre/modules/AddonRepository.jsm");
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://services-sync/async.js");
CU.import("resource://services-sync/util.js");
CU.import("resource://tps/logger.jsm");
var XPIProvider = CU.import("resource://gre/modules/XPIProvider.jsm")
                  .XPIProvider;

const ADDONSGETURL = 'http://127.0.0.1:4567/';
const STATE_ENABLED = 1;
const STATE_DISABLED = 2;

function GetFileAsText(file)
{
  let channel = Services.io.newChannel(file, null, null);
  let inputStream = channel.open();
  if (channel instanceof CI.nsIHttpChannel && 
      channel.responseStatus != 200) {
    return "";
  }

  let streamBuf = "";
  let sis = CC["@mozilla.org/scriptableinputstream;1"]
            .createInstance(CI.nsIScriptableInputStream);
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
  _addons_requiring_restart: [],
  _addons_pending_install: [],

  Delete: function() {
    // find our addon locally
    let cb = Async.makeSyncCallback();
    XPIProvider.getAddonsByTypes(null, cb);
    let results =  Async.waitForSyncCallback(cb);
    var addon;
    var id = this.id;
    results.forEach(function(result) {
      if (result.id == id) {
        addon = result;
      }
    });
    Logger.AssertTrue(!!addon, 'could not find addon ' + this.id + ' to uninstall');
    addon.uninstall();
  },

  Find: function(state) {
    let cb = Async.makeSyncCallback();
    let addon_found = false;
    var that = this;

    var log_addon = function(addon) {
      that.addon = addon;
      Logger.logInfo('addon ' + addon.id + ' found, isActive: ' + addon.isActive);
      if (state == STATE_ENABLED || state == STATE_DISABLED) {
          Logger.AssertEqual(addon.isActive,
            state == STATE_ENABLED ? true : false,
            "addon " + that.id + " has an incorrect enabled state");
      }
    };

    // first look in the list of all addons
    XPIProvider.getAddonsByTypes(null, cb);
    let addonlist = Async.waitForSyncCallback(cb);
    addonlist.forEach(function(addon) {
      if (addon.id == that.id) {
        addon_found = true;
        log_addon.call(that, addon);
      }
    });

    if (!addon_found) {
      // then look in the list of recent installs
      cb = Async.makeSyncCallback();
      XPIProvider.getInstallsByTypes(null, cb);
      addonlist = Async.waitForSyncCallback(cb);
      for (var i in addonlist) {
        if (addonlist[i].addon && addonlist[i].addon.id == that.id &&
            addonlist[i].state == AddonManager.STATE_INSTALLED) {
          addon_found = true;
          log_addon.call(that, addonlist[i].addon);
        }
      }
    }

    return addon_found;
  },

  Install: function() {
    // For Install, the id parameter initially passed is really the filename
    // for the addon's install .xml; we'll read the actual id from the .xml.
    let url = this.id;

    // set the url used by getAddonsByIDs
    var prefs = CC["@mozilla.org/preferences-service;1"]
                .getService(CI.nsIPrefBranch);
    prefs.setCharPref('extensions.getAddons.get.url', ADDONSGETURL + url);

    // read the XML and find the addon id
    xml = GetFileAsText(ADDONSGETURL + url);
    Logger.AssertTrue(xml.indexOf("<guid>") > -1, 'guid not found in ' + url);
    this.id = xml.substring(xml.indexOf("<guid>") + 6, xml.indexOf("</guid"));
    Logger.logInfo('addon XML = ' + this.id);

    // find our addon on 'AMO'
    let cb = Async.makeSyncCallback();
    AddonRepository.getAddonsByIDs([this.id], {
      searchSucceeded: cb,
      searchFailed: cb
    }, false);

    // Result will be array of addons on searchSucceeded or undefined on
    // searchFailed.
    let install_addons = Async.waitForSyncCallback(cb);

    Logger.AssertTrue(install_addons,
                      "no addons found for id " + this.id);
    Logger.AssertEqual(install_addons.length,
                       1,
                       "multiple addons found for id " + this.id);

    let addon = install_addons[0];
    Logger.logInfo(JSON.stringify(addon), null, ' ');
    if (XPIProvider.installRequiresRestart(addon)) {
      this._addons_requiring_restart.push(addon.id);
    }

    // Start installing the addon asynchronously; finish up in
    // onInstallEnded(), onInstallFailed(), or onDownloadFailed().
    this._addons_pending_install.push(addon.id);
    this.TPS.StartAsyncOperation();

    Utils.nextTick(function() {
      let callback = function(aInstall) {
        addon.install = aInstall;
        Logger.logInfo("addon install: " + addon.install);
        Logger.AssertTrue(addon.install,
                          "could not get install object for id " + this.id);
        addon.install.addListener(this);
        addon.install.install();
      };

      AddonManager.getInstallForURL(addon.sourceURI.spec,
                                    callback.bind(this),
                                    "application/x-xpinstall");
    }, this);
  },

  SetState: function(state) {
    if (!this.Find())
      return false;
    this.addon.userDisabled = state == STATE_ENABLED ? false : true;
      return true;
  },

  // addon installation callbacks
  onInstallEnded: function(addon) {
    try {
      Logger.logInfo('--------- event observed: addon onInstallEnded');
      Logger.AssertTrue(addon.addon,
        "No addon object in addon instance passed to onInstallEnded");
      Logger.AssertTrue(this._addons_pending_install.indexOf(addon.addon.id) > -1,
        "onInstallEnded received for unexpected addon " + addon.addon.id);
      this._addons_pending_install.splice(
        this._addons_pending_install.indexOf(addon.addon.id),
        1);
    }
    catch(e) {
      // We can't throw during a callback, as it will just get eaten by
      // the callback's caller.
      Utils.nextTick(function() {
        this.DumpError(e);
      }, this);
      return;
    }
    this.TPS.FinishAsyncOperation();
  },

  onInstallFailed: function(addon) {
    Logger.logInfo('--------- event observed: addon onInstallFailed');
    Utils.nextTick(function() {
      this.DumpError('Installation failed for addon ' + 
        (addon.addon && addon.addon.id ? addon.addon.id : 'unknown'));
    }, this);
  },

  onDownloadFailed: function(addon) {
    Logger.logInfo('--------- event observed: addon onDownloadFailed');
    Utils.nextTick(function() {
      this.DumpError('Download failed for addon ' + 
        (addon.addon && addon.addon.id ? addon.addon.id : 'unknown'));
    }, this);
  },

};
