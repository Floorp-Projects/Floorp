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
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/type_records/clientData.js");

Function.prototype.async = Async.sugar;

Utils.lazy(this, 'Clients', ClientEngine);

function ClientEngine() {
  this._init();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "clients",
  displayName: "Clients",
  logName: "Clients",
  _storeObj: ClientStore,
  _trackerObj: ClientTracker,
  _recordObj: ClientRecord
};

function ClientStore() {
  this._ClientStore_init();
}
ClientStore.prototype = {
  __proto__: Store.prototype,
  _logName: "Clients.Store",

  get GUID() this._getCharPref("client.GUID", function() Utils.makeGUID()),
  set GUID(value) Utils.prefs.setCharPref("client.GUID", value),

  get name() this._getCharPref("client.name", function() "Firefox"),
  set name(value) Utils.prefs.setCharPref("client.name", value),

  get type() this._getCharPref("client.type", function() "desktop"),
  set type(value) Utils.prefs.setCharPref("client.type", value),

  // FIXME: use Preferences module instead
  _getCharPref: function ClientData__getCharPref(pref, defaultCb) {
    let value;
    try {
      value = Utils.prefs.getCharPref(pref);
    } catch (e) {
      value = defaultCb();
      Utils.prefs.setCharPref(pref, value);
    }
    return value;
  },

  _ClientStore_init: function ClientStore__init() {
    this.__proto__.__proto__._init.call(this);
    this.clients = {};
    this.loadSnapshot();
  },

  saveSnapshot: function ClientStore_saveSnapshot() {
    this._log.debug("Saving client list to disk");
    let file = Utils.getProfileFile(
      {path: "weave/meta/clients.json", autoCreate: true});
    let out = Svc.Json.encode(this.clients);
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
      this.clients = Svc.Json.decode(json);
    } catch (e) {
      this._log.debug("Failed to load saved client list" + e);
    }
  },

  // methods to apply changes: create, remove, update, changeItemID, wipe

  create: function ClientStore_create(record) {
    this.update(record);
  },

  remove: function ClientStore_remove(record) {
    delete this.clients[record.id];
    this.saveSnapshot();
  },

  update: function ClientStore_update(record) {
    this.clients[record.id] = record.payload;
    this.saveSnapshot();
  },

  changeItemID: function ClientStore_changeItemID(oldID, newID) {
    this.clients[newID] = this.clients[oldID];
    delete this.clients[oldID];
    this.saveSnapshot();
  },

  wipe: function ClientStore_wipe() {
    this.clients = {};
    this.saveSnapshot();
  },

  // methods to query local data: getAllIDs, itemExists, createRecord

  getAllIDs: function ClientStore_getAllIDs() {
    // make sure we always return at least our GUID (e.g., before syncing
    // when the client list is empty);
    this.clients[this.GUID] = true;
    return this.clients;
  },

  itemExists: function ClientStore_itemExists(id) {
    return id in this.clients;
  },

  createRecord: function ClientStore_createRecord(guid) {
    let record = new ClientRecord();
    if (guid == this.GUID)
      record.payload = {name: this.name, type: this.type};
    else
      record.payload = this.clients[guid];
    return record;
  }
};

function ClientTracker() {
  this._init();
}
ClientTracker.prototype = {
  __proto__: Tracker.prototype,
  _logName: "ClientTracker",

  // we always want to sync, but are not dying to do so
  get score() "75", 

  // Override Tracker's _init to disable loading changed IDs from disk
  _init: function ClientTracker__init() {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this._store = new ClientStore(); // FIXME: hack
    this.enable();
  },

  // always upload our record
  // FIXME: we should compare against the store's snapshot instead
  get changedIDs() {
    let items = {};
    items[this._store.GUID] = true;
    return items;
  },

  // clobber these just in case anything tries to call them
  addChangedID: function(id) {},
  removeChangedID: function(id) {},
  clearChangedIDs: function() {}
};
