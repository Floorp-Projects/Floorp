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
Cu.import("resource://weave/type_records/clientData.js");

Utils.lazy(this, 'Clients', ClientEngine);

function ClientEngine() {
  this._ClientEngine_init();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "clients",
  _displayName: "Clients",
  description: "Sync information about other clients connected to Weave Sync",
  logName: "Clients",
  _storeObj: ClientStore,
  _trackerObj: ClientTracker,
  _recordObj: ClientRecord,

  _ClientEngine_init: function ClientEngine__init() {
    this._init();
    Utils.prefs.addObserver("", this, false);
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

  get clientName() {
    if (Svc.Prefs.isSet("client.name"))
      return Svc.Prefs.get("client.name");

    // Generate a client name if we don't have a useful one yet
    let user = Svc.Env.get("USER") || Svc.Env.get("USERNAME");
    let app = Svc.AppInfo.name;
    let host = Svc.SysInfo.get("host");

    // Try figuring out the name of the current profile
    let prof = Svc.Directory.get("ProfD", Components.interfaces.nsIFile).path;
    let profiles = Svc.Profiles.profiles;
    while (profiles.hasMoreElements()) {
      let profile = profiles.getNext().QueryInterface(Ci.nsIToolkitProfile);
      if (prof == profile.rootDir.path) {
        // Only bother adding the profile name if it's not "default"
        if (profile.name != "default")
          host = profile.name + "-" + host;
        break;
      }
    }

    return this.clientName = Str.sync.get("client.name", [user, app, host]);
  },
  set clientName(value) { Svc.Prefs.set("client.name", value); },

  get clientType() { return Svc.Prefs.get("client.type", "desktop"); },
  set clientType(value) { Svc.Prefs.set("client.type", value); },

  updateLocalInfo: function ClientEngine_updateLocalInfo(info) {
    // Grab data from the store if we weren't given something to start with
    if (!info)
      info = this.getInfo(this.clientID);

    // Overwrite any existing values with the ones from the pref
    info.name = this.clientName;
    info.type = this.clientType;

    return info;
  },

  observe: function ClientEngine_observe(subject, topic, data) {
    switch (topic) {
    case "nsPref:changed":
      switch (data) {
      case "client.name":
      case "client.type":
        // Update the store and tracker on pref changes
        this.setInfo(this.clientID, this.updateLocalInfo());
        break;
      }
      break;
    }
  },

  _resetClient: function ClientEngine__resetClient() {
    this.resetLastSync();
    this._store.wipe();
  }
};

function ClientStore() {
  this.init();
}
ClientStore.prototype = {
  //////////////////////////////////////////////////////////////////////////////
  // ClientStore Attributes

  clients: {},

  __proto__: Store.prototype,

  _snapshot: "meta/clients",

  //////////////////////////////////////////////////////////////////////////////
  // ClientStore Methods

  /**
   * Get the client by guid
   */
  getInfo: function ClientStore_getInfo(id) this.clients[id],

  /**
   * Initialize parent class then load client data from disk
   */
  init: function ClientStore_init() {
    this._init.call(this);
    this.loadSnapshot();

    // Get fresh local client info in case prefs were changed when closed
    let id = Clients.clientID;
    this.setInfo(id, Clients.updateLocalInfo(this.clients[id] || {}));
  },

  /**
   * Load client data from json disk
   */
  loadSnapshot: function ClientStore_loadSnapshot() {
    Utils.jsonLoad(this._snapshot, this, function(json) this.clients = json);
  },

  /**
   * Log that we're about to change the client store then save to disk
   */
  modify: function ClientStore_modify(message, action) {
    this._log.debug(message);
    action.call(this);
    this.saveSnapshot();
  },

  /**
   * Save client data to json disk
   */
  saveSnapshot: function ClientStore_saveSnapshot() {
    Utils.jsonSave(this._snapshot, this, this.clients);
  },

  /**
   * Set the client data for a guid. Use Engine.setInfo to update tracker.
   */
  setInfo: function ClientStore_setInfo(id, info) {
    this.modify("Setting client " + id + ": " + JSON.stringify(info),
      function() this.clients[id] = info);
  },

  //////////////////////////////////////////////////////////////////////////////
  // Store.prototype Attributes

  name: "clients",
  _logName: "Clients.Store",

  //////////////////////////////////////////////////////////////////////////////
  // Store.prototype Methods

  changeItemID: function ClientStore_changeItemID(oldID, newID) {
    this.modify("Changing id from " + oldId + " to " + newID, function() {
      this.clients[newID] = this.clients[oldID];
      delete this.clients[oldID];
    });
  },

  create: function ClientStore_create(record) {
    this.update(record);
  },

  createRecord: function ClientStore_createRecord(id) {
    let record = new ClientRecord();
    record.id = id;
    record.payload = this.clients[id];

    return record;
  },

  getAllIDs: function ClientStore_getAllIDs() this.clients,

  itemExists: function ClientStore_itemExists(id) id in this.clients,

  remove: function ClientStore_remove(record) {
    this.modify("Removing client " + record.id, function()
      delete this.clients[record.id]);
  },

  update: function ClientStore_update(record) {
    this.modify("Updating client " + record.id, function()
      this.clients[record.id] = record.payload);
  },

  wipe: function ClientStore_wipe() {
    this.modify("Wiping local clients store", function() {
      this.clients = {};

      // Make sure the local client is still here
      this.clients[Clients.clientID] = Clients.updateLocalInfo({});
    });
  },
};

function ClientTracker() {
  this._init();
}
ClientTracker.prototype = {
  __proto__: Tracker.prototype,
  name: "clients",
  _logName: "ClientTracker",
  file: "clients",
  get score() 100 // always sync
};
