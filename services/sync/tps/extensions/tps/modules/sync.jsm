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
 * The Original Code is TPS.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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


var EXPORTED_SYMBOLS = ["TPS", "SYNC_WIPE_SERVER", "SYNC_RESET_CLIENT",
                        "SYNC_WIPE_CLIENT"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://gre/modules/XPCOMUtils.jsm");
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://tps/logger.jsm");
CU.import("resource://services-sync/service.js");
CU.import("resource://services-sync/util.js");
var utils = {}; CU.import('resource://mozmill/modules/utils.js', utils);

const SYNC_WIPE_SERVER = "wipe-server";
const SYNC_RESET_CLIENT = "reset-client";
const SYNC_WIPE_CLIENT = "wipe-client";

var prefs = CC["@mozilla.org/preferences-service;1"]
            .getService(CI.nsIPrefBranch);

var syncFinishedCallback = function() {
  Logger.logInfo('syncFinishedCallback returned ' + !TPS._waitingForSync);
  return !TPS._waitingForSync;
};

var TPS = {
  _waitingForSync: false,
  _syncErrors: 0,

  QueryInterface: XPCOMUtils.generateQI([CI.nsIObserver,
                                         CI.nsISupportsWeakReference]),

  observe: function TPS__observe(subject, topic, data) {
    Logger.logInfo('Mozmill observed: ' + topic);
    switch(topic) {
      case "weave:service:sync:error":
        if (this._waitingForSync && this._syncErrors == 0) {
          Logger.logInfo("sync error; retrying...");
          this._syncErrors++;
          Utils.namedTimer(function() {
            Weave.service.sync();
          }, 1000, this, "resync");
        }
        else if (this._waitingForSync) {
          this._syncErrors = "sync error, see log";
          this._waitingForSync = false;
        }
        break;
      case "weave:service:sync:finish":
        if (this._waitingForSync) {
          this._syncErrors = 0;
          this._waitingForSync = false;
        }
        break;
    }
  },

  SetupSyncAccount: function TPS__SetupSyncAccount() {
    Weave.Service.account = prefs.getCharPref('tps.account.username');
    Weave.Service.password = prefs.getCharPref('tps.account.password');
    Weave.Service.passphrase = prefs.getCharPref('tps.account.passphrase');
    Weave.Svc.Obs.notify("weave:service:setup-complete");
  },

  Sync: function TPS__Sync(options) {
    Logger.logInfo('Mozmill starting sync operation: ' + options);
    switch(options) {
      case SYNC_WIPE_SERVER:
        Weave.Svc.Prefs.set("firstSync", "wipeRemote");
        break;
      case SYNC_WIPE_CLIENT:
        Weave.Svc.Prefs.set("firstSync", "wipeClient");
        break;
      case SYNC_RESET_CLIENT:
        Weave.Svc.Prefs.set("firstSync", "resetClient");
        break;
      default:
        Weave.Svc.Prefs.reset("firstSync");
    }

    if (Weave.Status.service != Weave.STATUS_OK) {
      return "Sync status not ok: " + Weave.Status.service;
    }

    this._waitingForSync = true;
    this._syncErrors = 0;
    Weave.Service.sync();
    utils.waitFor(syncFinishedCallback, null, 20000, 500, TPS);
    return this._syncErrors;
  },
};

Services.obs.addObserver(TPS, "weave:service:sync:finish", true);
Services.obs.addObserver(TPS, "weave:service:sync:error", true);
Logger.init();


