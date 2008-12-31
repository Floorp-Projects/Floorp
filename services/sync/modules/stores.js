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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

const EXPORTED_SYMBOLS = ['Store',
                          'SnapshotStore'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

/*
 * Data Stores
 * These can wrap, serialize items and apply commands
 */

function Store() {
  this._init();
}
Store.prototype = {
  _logName: "BaseClass",

  // set this property in child object's wrap()!
  _lookup: null,

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  __json: null,
  get _json() {
    if (!this.__json)
      this.__json = Cc["@mozilla.org/dom/json;1"].
        createInstance(Ci.nsIJSON);
    return this.__json;
  },

  get cache() {
    let cache = new RecordCache();
    this.__defineGetter__("cache", function() cache);
    return cache;
  },

  _init: function Store__init() {
    this._log = Log4Moz.repository.getLogger("Store." + this._logName);
  },

  applyIncoming: function BStore_applyIncoming(onComplete, record) {
    let fn = function(rec) {
      let self = yield;
      if (!rec.cleartext)
        this.remove(rec);
      else if (!this.itemExists(rec.id))
        this.create(rec);
      else
        this.update(rec);
    };
    fn.async(this, onComplete, record);
  },

  // override these in derived objects

  itemExists: function Store_itemExists(id) {
    throw "override itemExists in a subclass";
  },

  createRecord: function Store_createRecord() {
    throw "override createRecord in a subclass";
  },

  wrapDepth: function BStore_wrapDepth(guid, items) {
    if (typeof(items) == "undefined")
      items = {};
    for (let childguid in this._itemCache) {
      if (this._itemCache[childguid].parentid == guid) {
        items[childguid] = this._itemCache[childguid].depth;
        this.wrapDepth(childguid, items);
      }
    }
    return items;
  },

  changeItemID: function Store_changeItemID(oldID, newID) {
    throw "override changeItemID in a subclass";
  },

  getAllIDs: function Store_getAllIDs() {
    throw "override getAllIDs in a subclass";
  },

  wipe: function Store_wipe() {
    throw "override wipe in a subclass";
  }
};

function Cache() {
  this.count = 0;
  this.maxItems = 100;
  this.fifo = true;
  this.enabled = true;
  this._head = this._tail = null;
  this._items = {};
}
Cache.prototype = {
  _pop: function Cache__pop() {
    if (this.count <= 0)
      return;
    if (this.count == 1)
      this.clear();
    else {
      delete this._items[this._tail.id];
      this._tail = this._tail.prev;
      this.count--;
    }
  },
  put: function Cache_put(id, item) {
    if (!this.enabled)
      return;
    let wrapper = {id: id, prev: null, next: this._head, item: item};
    this._items[wrapper.id] = wrapper;

    if (this.fifo) {
      if (this._head)
        this._head.prev = wrapper;
      this._head = wrapper;
    } else {
      if (this._tail)
        this._tail.next = wrapper;
      this._tail = wrapper;
    }
    
    this.count++;
    if (this.count >= this.maxItems)
      this._pop();
  },
  get: function Cache_get(id) {
    if (id in this._items)
      return this._items[id].item;
    return undefined;
  },
  clear: function Cache_clear() {
    this.count = 0;
    this._head = null;
    this._tail = null;
    this._items = {};
  }
};

function SnapshotStore(name) {
  this._init(name);
}
SnapshotStore.prototype = {
  _logName: "SnapStore",

  _filename: null,
  get filename() {
    if (this._filename === null)
      throw "filename is null";
    return this._filename;
  },
  set filename(value) {
    this._filename = value + ".json";
  },

  // Last synced tree, version, and GUID (to detect if the store has
  // been completely replaced and invalidate the snapshot)

  _data: {},
  get data() { return this._data; },
  set data(value) { this._data = value; },

  _version: 0,
  get version() { return this._version; },
  set version(value) { this._version = value; },

  _GUID: null,
  get GUID() {
    if (!this._GUID)
      this._GUID = Utils.makeGUID();
    return this._GUID;
  },
  set GUID(GUID) {
    this._GUID = GUID;
  },

  _init: function SStore__init(name) {
    this.filename = name;
    this._log = Log4Moz.repository.getLogger("Service." + this._logName);
  },

  _createCommand: function SStore__createCommand(command) {
    this._data[command.GUID] = Utils.deepCopy(command.data);
  },

  _removeCommand: function SStore__removeCommand(command) {
    delete this._data[command.GUID];
  },

  _editCommand: function SStore__editCommand(command) {
    if ("GUID" in command.data) {
      // special-case guid changes
      let newGUID = command.data.GUID,
          oldGUID = command.GUID;

      this._data[newGUID] = this._data[oldGUID];
      delete this._data[oldGUID];

      for (let GUID in this._data) {
        if (("parentid" in this._data[GUID]) &&
            (this._data[GUID].parentid == oldGUID))
          this._data[GUID].parentid = newGUID;
      }
    }
    for (let prop in command.data) {
      if (prop == "GUID")
        continue;
      if (command.GUID in this._data)
        this._data[command.GUID][prop] = command.data[prop];
      else
        this._log.warn("Warning! Edit command for unknown item: " + command.GUID);
    }
  },

  save: function SStore_save() {
    this._log.info("Saving snapshot to disk");

    let file = Utils.getProfileFile(
      {path: "weave/snapshots/" + this.filename,
       autoCreate: true}
      );

    let out = {version: this.version,
               GUID: this.GUID,
               snapshot: this.data};
    out = this._json.encode(out);

    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
  },

  load: function SStore_load() {
    let file = Utils.getProfileFile("weave/snapshots/" + this.filename);
    if (!file.exists())
      return;

    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      json = this._json.decode(json);

      if (json && 'snapshot' in json && 'version' in json && 'GUID' in json) {
        this._log.debug("Read saved snapshot from disk");
        this.data = json.snapshot;
        this.version = json.version;
        this.GUID = json.GUID;
      }
    } catch (e) {
      this._log.warn("Could not parse saved snapshot; reverting to initial-sync state");
      this._log.debug("Exception: " + e);
    }
  },

  serialize: function SStore_serialize() {
    let json = this._json.encode(this.data);
    json = json.replace(/:{type/g, ":\n\t{type");
    json = json.replace(/}, /g, "},\n  ");
    json = json.replace(/, parentid/g, ",\n\t parentid");
    json = json.replace(/, index/g, ",\n\t index");
    json = json.replace(/, title/g, ",\n\t title");
    json = json.replace(/, URI/g, ",\n\t URI");
    json = json.replace(/, tags/g, ",\n\t tags");
    json = json.replace(/, keyword/g, ",\n\t keyword");
    return json;
  },

  wrap: function SStore_wrap() {
    return this.data;
  },

  wipe: function SStore_wipe() {
    this.data = {};
    this.version = -1;
    this.GUID = null;
    this.save();
  }
};
SnapshotStore.prototype.__proto__ = new Store();
