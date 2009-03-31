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
 * The Original Code is Weave
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ['Clients'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/type_records/clientData.js");

Function.prototype.async = Async.sugar;

Utils.lazy(this, 'Clients', ClientEngine);

function ClientEngine() {
  this._ClientEngine_init();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "clients",
  displayName: "Clients",
  logName: "Clients",
  _storeObj: ClientStore,
  _trackerObj: ClientTracker,
  _recordObj: ClientRecord,

  _ClientEngine_init: function ClientEngine__init() {
    this._init();
    Utils.prefs.addObserver("", this, false);
  },

  // We never want to change one client id to another, even if
  // they look exactly the same
  _recordLike: function SyncEngine__recordLike(a, b) {
    return false;
  },

  // get and set info for clients

  // FIXME: callers must use the setInfo interface or changes won't get synced,
  // which is unintuitive

  getClients: function ClientEngine_getClients() {
    return this._store.clients;
  },

  getInfo: function ClientEngine_getInfo(id) {
    return this._store.getInfo(id);
  },

  setInfo: function ClientEngine_setInfo(id, info) {
    this._store.setInfo(id, info);
    this._tracker.addChangedID(id);
  },

  // helpers for getting/setting this client's info directly

  get clientID() {
    if (!Svc.Prefs.get("client.GUID"))
      Svc.Prefs.set("client.GUID", Utils.makeGUID());
    return Svc.Prefs.get("client.GUID");
  },

  get syncID() {
    if (!Svc.Prefs.get("client.syncID"))
      Svc.Prefs.set("client.syncID", Utils.makeGUID());
    return Svc.Prefs.get("client.syncID");
  },
  set syncID(value) {
    Svc.Prefs.set("client.syncID", value);
  },
  resetSyncID: function ClientEngine_resetSyncID() {
    Svc.Prefs.reset("client.syncID");
  },

  get clientName() { return Svc.Prefs.get("client.name", "Firefox"); },
  set clientName(value) { Svc.Prefs.set("client.name", value); },

  get clientType() { return Svc.Prefs.get("client.type", "desktop"); },
  set clientType(value) { Svc.Prefs.set("client.type", value); },

  observe: function ClientEngine_observe(subject, topic, data) {
    switch (topic) {
    case "nsPref:changed":
      switch (data) {
      case "client.name":
      case "client.type":
        this._tracker.addChangedID(this.clientID);
        break;
      }
      break;
    }
  },

  _resetClient: function ClientEngine__resetClient() {
    let self = yield;
    this.resetLastSync();
    this._store.wipe();
  }
};

function ClientStore() {
  this._ClientStore_init();
}
ClientStore.prototype = {
  __proto__: Store.prototype,
  _logName: "Clients.Store",

  _ClientStore_init: function ClientStore__init() {
    this._init.call(this);
    this._clients = {};
    this.loadSnapshot();
  },

  get clients() {
    this._clients[Clients.clientID] = this.createRecord(Clients.clientID).payload;
    return this._clients;
  },

  // get/set client info; doesn't use records like the methods below
  // NOTE: setInfo will not update tracker.  Use Engine.setInfo for that

  getInfo: function ClientStore_getInfo(id) {
    return this._clients[id];
  },

  setInfo: function ClientStore_setInfo(id, info) {
    this._clients[id] = info;
    this.saveSnapshot();
  },

  // load/save clients list from/to disk

  saveSnapshot: function ClientStore_saveSnapshot() {
    this._log.debug("Saving client list to disk");
    let file = Utils.getProfileFile(
      {path: "weave/meta/clients.json", autoCreate: true});
    let out = JSON.stringify(this._clients);
    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
  },

  loadSnapshot: function ClientStore_loadSnapshot() {
    let file = Utils.getProfileFile("weave/meta/clients.json");
    if (!file.exists())
      return;
    this._log.debug("Loading client list from disk");
    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      this._clients = JSON.parse(json);
    } catch (e) {
      this._log.debug("Failed to load saved client list" + e);
    }
  },

  // methods to apply changes: create, remove, update, changeItemID, wipe

  applyIncoming: function ClientStore_applyIncoming(onComplete, record) {
    let fn = function(rec) {
      let self = yield;
      if (!rec.payload)
        this.remove(rec);
      else if (!this.itemExists(rec.id))
        this.create(rec);
      else
        this.update(rec);
    };
    fn.async(this, onComplete, record);
  },

  create: function ClientStore_create(record) {
    this.update(record);
  },

  update: function ClientStore_update(record) {
    this._log.debug("Updating client " + record.id);
    this._clients[record.id] = record.payload;
    this.saveSnapshot();
  },

  remove: function ClientStore_remove(record) {
    this._log.debug("Removing client " + record.id);
    delete this._clients[record.id];
    this.saveSnapshot();
  },

  changeItemID: function ClientStore_changeItemID(oldID, newID) {
    this._clients[newID] = this._clients[oldID];
    delete this._clients[oldID];
    this.saveSnapshot();
  },

  wipe: function ClientStore_wipe() {
    this._log.debug("Wiping local clients store");
    this._clients = {};
    this.saveSnapshot();
  },

  // methods to query local data: getAllIDs, itemExists, createRecord

  getAllIDs: function ClientStore_getAllIDs() {
    return this.clients;
  },

  itemExists: function ClientStore_itemExists(id) {
    return id in this._clients;
  },

  createRecord: function ClientStore_createRecord(id) {
    let record = new ClientRecord();
    record.id = id;
    record.payload = this._clients[id] || {};

    // For the local client, update the name and type with the current value
    if (id == Clients.clientID) {
      record.payload.name = Clients.clientName;
      record.payload.type = Clients.clientType;
    }

    return record;
  }
};

function ClientTracker() {
  this._init();
}
ClientTracker.prototype = {
  __proto__: Tracker.prototype,
  _logName: "ClientTracker",
  file: "clients",
  get score() 100 // always sync
};
