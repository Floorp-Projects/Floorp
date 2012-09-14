/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var EXPORTED_SYMBOLS = ["TPS", "SYNC_WIPE_SERVER", "SYNC_RESET_CLIENT",
                        "SYNC_WIPE_CLIENT"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://gre/modules/XPCOMUtils.jsm");
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://services-sync/util.js");
CU.import("resource://tps/logger.jsm");
var utils = {}; CU.import('resource://mozmill/modules/utils.js', utils);

const SYNC_RESET_CLIENT = "reset-client";
const SYNC_WIPE_CLIENT  = "wipe-client";
const SYNC_WIPE_REMOTE  = "wipe-remote";
const SYNC_WIPE_SERVER  = "wipe-server";

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
    try {
      let serverURL = prefs.getCharPref('tps.account.serverURL');
      if (serverURL) {
        Weave.Service.serverURL = serverURL;
      }
    }
    catch(e) {}
    Weave.Service.identity.account       = prefs.getCharPref('tps.account.username');
    Weave.Service.Identity.basicPassword = prefs.getCharPref('tps.account.password');
    Weave.Service.identity.syncKey       = prefs.getCharPref('tps.account.passphrase');
    Weave.Svc.Obs.notify("weave:service:setup-complete");
  },

  Sync: function TPS__Sync(options) {
    Logger.logInfo('Mozmill starting sync operation: ' + options);
    switch(options) {
      case SYNC_WIPE_REMOTE:
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

    this._syncErrors = 0;

    if (options == SYNC_WIPE_SERVER) {
      Weave.Service.wipeServer();
    } else {
      this._waitingForSync = true;
      Weave.Service.sync();
      utils.waitFor(syncFinishedCallback, null, 20000, 500, TPS);
    }
    return this._syncErrors;
  },
};

Services.obs.addObserver(TPS, "weave:service:sync:finish", true);
Services.obs.addObserver(TPS, "weave:service:sync:error", true);
Logger.init();


