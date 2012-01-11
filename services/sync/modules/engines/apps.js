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
 * The Original Code is Apps Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Fabrice Desré <fabrice@mozilla.com>
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

const EXPORTED_SYMBOLS = ['AppsEngine', 'AppRec'];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");

function AppRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}

AppRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.App"
}

Utils.deferGetSet(AppRec, "cleartext", ["value"]);

function AppStore(name) {
  Store.call(this, name);
}

AppStore.prototype = {
  __proto__: Store.prototype,

  getAllIDs: function getAllIDs() {
    let apps = DOMApplicationRegistry.getAllIDs();
    return apps;
  },

  changeItemID: function changeItemID(oldID, newID) {
    this._log.trace("AppsStore does not support changeItemID");
  },

  itemExists: function itemExists(guid) {
    return DOMApplicationRegistry.itemExists(guid);
  },

  createRecord: function createRecord(guid, collection) {
    let record = new AppRec(collection, guid);
    let app = DOMApplicationRegistry.getAppById(guid);
    
    if (app) {
      app.syncId = guid;
      let callback = Async.makeSyncCallback();
      DOMApplicationRegistry.getManifestFor(app.origin, function(aManifest) {
        app.manifest = aManifest;
        callback();
      });
      Async.waitForSyncCallback(callback);
      record.value = app;
    } else {
      record.deleted = true;
    }

    return record;
  },

  applyIncomingBatch: function applyIncomingBatch(aRecords) {
    let callback = Async.makeSyncCallback();
    DOMApplicationRegistry.updateApps(aRecords, callback);
    Async.waitForSyncCallback(callback);
    return [];
  },

  wipe: function wipe(record) {
    let callback = Async.makeSyncCallback();
    DOMApplicationRegistry.wipe(callback);
    Async.waitForSyncCallback(callback);
  }
}


function AppTracker(name) {
  Tracker.call(this, name);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}

AppTracker.prototype = {
  __proto__: Tracker.prototype,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  
  _enabled: false,

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "webapps-sync-install":
      case "webapps-sync-uninstall":
        // ask for immediate sync. not sure if we really need this or
        // if a lower score increment would be enough
        this.score += SCORE_INCREMENT_XLARGE;
        this.addChangedID(aData);
        break;
      case "weave:engine:start-tracking":
        this._enabled = true;
        Svc.Obs.add("webapps-sync-install", this);
        Svc.Obs.add("webapps-sync-uninstall", this);
        break;
      case "weave:engine:stop-tracking":
        this._enabled = false;
        Svc.Obs.remove("webapps-sync-install", this);
        Svc.Obs.remove("webapps-sync-uninstall", this);
        break;
    }
  }
}

function AppsEngine() {
  SyncEngine.call(this, "Apps");
}

AppsEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: AppStore,
  _trackerObj: AppTracker,
  _recordObj: AppRec,
  applyIncomingBatchSize: APPS_STORE_BATCH_SIZE
}
