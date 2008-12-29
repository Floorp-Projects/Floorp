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
 *  Myk Melez <myk@mozilla.org>
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

const EXPORTED_SYMBOLS = ['Engines', 'NewEngine', 'Engine', 'SyncEngine', 'BlobEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/clientData.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/async.js");

Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/keys.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/collection.js");

Function.prototype.async = Async.sugar;

// Singleton service, holds registered engines

Utils.lazy(this, 'Engines', EngineManagerSvc);

function EngineManagerSvc() {
  this._engines = {};
}
EngineManagerSvc.prototype = {
  get: function EngMgr_get(name) {
    return this._engines[name];
  },
  getAll: function EngMgr_getAll() {
    let ret = [];
    for (key in this._engines) {
      ret.push(this._engines[key]);
    }
    return ret;
  },
  getEnabled: function EngMgr_getEnabled() {
    let ret = [];
    for (key in this._engines) {
      if(this._engines[key].enabled)
        ret.push(this._engines[key]);
    }
    return ret;
  },
  register: function EngMgr_register(engine) {
    this._engines[engine.name] = engine;
  },
  unregister: function EngMgr_unregister(val) {
    let name = val;
    if (val instanceof Engine)
      name = val.name;
    delete this._engines[name];
  }
};

function Engine() { /* subclasses should call this._init() */}
Engine.prototype = {
  _notify: Wrap.notify,

  // "default-engine";
  get name() { throw "name property must be overridden in subclasses"; },

  // "Default";
  get displayName() { throw "displayName property must be overriden in subclasses"; },

  // "DefaultEngine";
  get logName() { throw "logName property must be overridden in subclasses"; },

  // "user-data/default-engine/";
  get serverPrefix() { throw "serverPrefix property must be overridden in subclasses"; },

  get enabled() {
    return Utils.prefs.getBoolPref("engine." + this.name);
  },

  get _os() {
    let os = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
    this.__defineGetter__("_os", function() os);
    return os;
  },

  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].
      createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() json);
    return json;
  },

  get score() this._tracker.score,

  // _store, and tracker need to be overridden in subclasses
  get _store() {
    let store = new Store();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  get _tracker() {
    let tracker = new Tracker();
    this.__defineGetter__("_tracker", function() tracker);
    return tracker;
  },

  get engineId() {
    let id = ID.get('Engine:' + this.name);
    if (!id) {
      // Copy the service login from WeaveID
      let masterID = ID.get('WeaveID');

      id = new Identity(this.logName, masterID.username, masterID.password);
      ID.set('Engine:' + this.name, id);
    }
    return id;
  },

  _init: function Engine__init() {
    let levelPref = "log.logger.service.engine." + this.name;
    let level = "Debug";
    try { level = Utils.prefs.getCharPref(levelPref); }
    catch (e) { /* ignore unset prefs */ }

    this._log = Log4Moz.repository.getLogger("Engine." + this.logName);
    this._log.level = Log4Moz.Level[level];
    this._osPrefix = "weave:" + this.name + ":";

    this._tracker; // initialize tracker to load previously changed IDs

    this._log.debug("Engine initialized");
  },

  _serializeCommands: function Engine__serializeCommands(commands) {
    let json = this._json.encode(commands);
    //json = json.replace(/ {action/g, "\n {action");
    return json;
  },

  _serializeConflicts: function Engine__serializeConflicts(conflicts) {
    let json = this._json.encode(conflicts);
    //json = json.replace(/ {action/g, "\n {action");
    return json;
  },

  _resetServer: function Engine__resetServer() {
    let self = yield;
    throw "_resetServer needs to be subclassed";
  },

  _resetClient: function Engine__resetClient() {
    let self = yield;
    this._log.debug("Resetting client state");
    this._store.wipe();
    this._log.debug("Client reset completed successfully");
  },

  _sync: function Engine__sync() {
    let self = yield;
    throw "_sync needs to be subclassed";
  },

  _share: function Engine__share(guid, username) {
    let self = yield;
    /* This should be overridden by the engine subclass for each datatype.
       Implementation should share the data node identified by guid,
       and all its children, if any, with the user identified by username. */
    self.done();
  },

  _stopSharing: function Engine__stopSharing(guid, username) {
    let self = yield;
    /* This should be overridden by the engine subclass for each datatype.
     Stop sharing the data node identified by guid with the user identified
     by username.*/
    self.done();
  },

  sync: function Engine_sync(onComplete) {
    return this._sync.async(this, onComplete);
  },

  share: function Engine_share(onComplete, guid, username) {
    return this._share.async(this, onComplete, guid, username);
  },

  stopSharing: function Engine_share(onComplete, guid, username) {
    return this._stopSharing.async(this, onComplete, guid, username);
  },

  resetServer: function Engimne_resetServer(onComplete) {
    this._notify("reset-server", "", this._resetServer).async(this, onComplete);
  },

  resetClient: function Engine_resetClient(onComplete) {
    this._notify("reset-client", "", this._resetClient).async(this, onComplete);
  }
};

function SyncEngine() { /* subclasses should call this._init() */ }
SyncEngine.prototype = {
  __proto__: Engine.prototype,

  get baseURL() {
    let url = Utils.prefs.getCharPref("serverURL");
    if (url && url[url.length-1] != '/')
      url += '/';
    url += "0.3/user/";
    return url;
  },

  get engineURL() {
    return this.baseURL + ID.get('WeaveID').username + '/' + this.name + '/';
  },

  get cryptoMetaURL() {
    return this.baseURL + ID.get('WeaveID').username + '/crypto/' + this.name;
  },

  get lastSync() {
    try {
      return Utils.prefs.getCharPref(this.name + ".lastSync");
    } catch (e) {
      return 0;
    }
  },
  set lastSync(value) {
    Utils.prefs.setCharPref(this.name + ".lastSync", value);
  },

  // XXX these two should perhaps just be a variable inside sync(), but we have
  //     one or two other methods that use it

  get incoming() {
    if (!this._incoming)
      this._incoming = [];
    return this._incoming;
  },

  get outgoing() {
    if (!this._outgoing)
      this._outgoing = [];
    return this._outgoing;
  },

  // Create a new record by querying the store, and add the engine metadata
  _createRecord: function SyncEngine__createRecord(id) {
    let record = this._store.createRecord(id);
    record.uri = this.engineURL + id;
    record.encryption = this.cryptoMetaURL;
    return record;
  },

  // Check if a record is "like" another one, even though the IDs are different,
  // in that case, we'll change the ID of the local item to match
  // Probably needs to be overridden in a subclass, to change which criteria
  // make two records "the same one"
  _recordLike: function SyncEngine__recordLike(a, b) {
    if (a.parentid != b.parentid)
      return false;
    if (a.depth != b.depth)
      return false;
    // note: sortindex ignored
    return Utils.deepEquals(a.cleartext, b.cleartext);
  },

  _changeRecordRefs: function SyncEngine__changeRecordRefs(oldID, newID) {
    let self = yield;
    for each (let rec in this.outgoing) {
      if (rec.parentid == oldID)
        rec.parentid = newID;
    }
  },

  // Any setup that needs to happen at the beginning of each sync.
  // Makes sure crypto records and keys are all set-up
  _syncStartup: function SyncEngine__syncStartup() {
    let self = yield;

    this._log.debug("Ensuring server crypto records are there");

    let meta = yield CryptoMetas.get(self.cb, this.cryptoMetaURL);
    if (!meta) {
      let cryptoSvc = Cc["@labs.mozilla.com/Weave/Crypto;1"].
        getService(Ci.IWeaveCrypto);
      let symkey = cryptoSvc.generateRandomKey();
      let pubkey = yield PubKeys.getDefaultKey(self.cb);
      meta = new CryptoMeta(this.cryptoMetaURL);
      meta.generateIV();
      yield meta.addUnwrappedKey(self.cb, pubkey, symkey);
      yield meta.put(self.cb);
    }
    this._tracker.disable();
  },

  // Generate outgoing records
  _generateOutgoing: function SyncEngine__generateOutgoing() {
    let self = yield;

    this._log.debug("Calculating client changes");

    // first sync special case: upload all items
    // note that we use a backdoor (of sorts) to the tracker and it
    // won't save to disk this list
    if (!this.lastSync) {
      this._log.info("First sync, uploading all items");

      // remove any old ones first
      this._tracker.clearChangedIDs();

      // now add all current ones
      let all = this._store.getAllIDs();
      for (let id in all) {
        this._tracker.changedIDs[id] = true;
      }
    }

    // generate queue from changed items list

    // XXX should have a heuristic like this, but then we need to be able to
    // serialize each item by itself, something our stores can't currently do
    //if (this._tracker.changedIDs.length >= 30)
    //this._store.cacheItemsHint();

    // NOTE we want changed items -> outgoing -> server to be as atomic as
    // possible, so we clear the changed IDs after we upload the changed records
    // NOTE2 don't encrypt, we'll do that before uploading instead
    for (let id in this._tracker.changedIDs) {
      this.outgoing.push(this._createRecord(id));
    }

    //this._store.clearItemCacheHint();
  },

  // Generate outgoing records
  _processIncoming: function SyncEngine__processIncoming() {
    let self = yield;

    this._log.debug("Downloading & applying server changes");

    let newitems = new Collection(this.engineURL);
    newitems.modified = this.lastSync;
    newitems.full = true;
    newitems.sort = "depthindex";
    yield newitems.get(self.cb);

    let mem = Cc["@mozilla.org/xpcom/memory-service;1"].getService(Ci.nsIMemory);
    this._lastSyncTmp = 0;
    let item;
    while ((item = yield newitems.iter.next(self.cb))) {
      if (mem.isLowMemory()) {
        this._log.warn("Low memory, forcing GC");
        Cu.forceGC();
        if (mem.isLowMemory()) {
          this._log.warn("Low memory, aborting sync!");
          throw "Low memory";
        }
      }
      yield item.decrypt(self.cb, ID.get('WeaveCryptoID').password);
      if (yield this._reconcile.async(this, self.cb, item))
        yield this._applyIncoming.async(this, self.cb, item);
      else
        this._log.debug("Skipping reconciled incoming item");
    }
    if (typeof(this._lastSyncTmp) == "string")
      this._lastSyncTmp = parseInt(this._lastSyncTmp);
    if (this.lastSync < this._lastSyncTmp)
        this.lastSync = this._lastSyncTmp;

    // removes any holes caused by reconciliation above:
    this._outgoing = this.outgoing.filter(function(n) n);
  },

  // Reconciliation has two steps:
  // 1) Check for the same item (same ID) on both the incoming and outgoing
  // queues.  This means the same item was modified on this profile and another
  // at the same time.  In this case, this client wins (which really means, the
  // last profile you sync wins).
  // 2) Check if any incoming & outgoing items are actually the same, even
  // though they have different IDs.  This happens when the same item is added
  // on two different machines at the same time.  For example, when a profile
  // is synced for the first time after having (manually or otherwise) imported
  // bookmarks imported, every bookmark will match this condition.
  // When two items with different IDs are "the same" we change the local ID to
  // match the remote one.
  _reconcile: function SyncEngine__reconcile(item) {
    let self = yield;
    let ret = true;

    this._log.debug("Reconciling incoming item");

    // Check for the same item (same ID) on both incoming & outgoing queues
    let conflicts = [];
    for (let o = 0; o < this.outgoing.length; o++) {
      if (!this.outgoing[o])
        continue; // skip previously removed items
      if (item.id == this.outgoing[o].id) {
        // Only consider it a conflict if there are actual differences
        // otherwise, just ignore the outgoing record as well
        if (!Utils.deepEquals(item.cleartext, this.outgoing[o].cleartext))
          conflicts.push({in: item, out: this.outgoing[o]});
        else
          delete this.outgoing[o];

        self.done(false);
        return;
      }
    }
    if (conflicts.length)
      this._log.debug("Conflicts found.  Conflicting server changes discarded");

    // Check for items with different IDs which we think are the same one
    for (let o = 0; o < this.outgoing.length; o++) {
      if (!this.outgoing[o])
        continue; // skip previously removed items

      if (this._recordLike(item, this.outgoing[o])) {
        // change refs in outgoing queue
        yield this._changeRecordRefs.async(this, self.cb,
                                           this.outgoing[o].id,
                                           item.id);
        // change actual id of item
        this._store.changeItemID(this.outgoing[o].id,
                                 item.id);
        delete this.outgoing[o];
        self.done(false);
        return;
      }
    }
    self.done(true);
  },

  // Apply incoming records
  _applyIncoming: function SyncEngine__applyIncoming(item) {
    let self = yield;
    this._log.debug("Applying incoming record");
    this._log.trace("Incoming:\n" + item);
    try {
      yield this._store.applyIncoming(self.cb, item);
      if (this._lastSyncTmp < item.modified)
        this._lastSyncTmp = item.modified;
    } catch (e) {
      this._log.warn("Error while applying incoming record: " +
                     (e.message? e.message : e));
    }
  },

  // Upload outgoing records
  _uploadOutgoing: function SyncEngine__uploadOutgoing() {
    let self = yield;

    if (this.outgoing.length) {
      this._log.debug("Uploading client changes (" + this.outgoing.length + ")");

      // collection we'll upload
      let up = new Collection(this.engineURL);

      // regen the store cache so we can get item depths
      //this._store.cacheItemsHint();
      let depth = {};

      let out;
      while ((out = this.outgoing.pop())) {
        this._log.trace("Outgoing:\n" + out);
        yield out.encrypt(self.cb, ID.get('WeaveCryptoID').password);
        yield up.pushRecord(self.cb, out);
        this._store.wrapDepth(out.id, depth);
      }

      // now add short depth-only records
      this._log.trace(depth.length + "outgoing depth records");
      for (let id in depth) {
        up.pushDepthRecord({id: id, depth: depth[id]});
      }
      //this._store.clearItemCacheHint();

      // do the upload
      yield up.post(self.cb);

      // save last modified date
      let mod = up.data.modified;
      if (typeof(mod) == "string")
        mod = parseInt(mod);
      if (mod > this.lastSync)
        this.lastSync = mod;
    }
    this._tracker.clearChangedIDs();
  },

  // Any cleanup necessary.
  // Save the current snapshot so as to calculate changes at next sync
  _syncFinish: function SyncEngine__syncFinish(error) {
    let self = yield;
    this._log.debug("Finishing up sync");
    this._tracker.resetScore();
    this._tracker.enable();
  },

  _sync: function SyncEngine__sync() {
    let self = yield;

    try {
      yield this._syncStartup.async(this, self.cb);

      // Populate outgoing queue
      yield this._generateOutgoing.async(this, self.cb);

      // Fetch incoming records and apply them
      yield this._processIncoming.async(this, self.cb);

      // Upload outgoing records
      yield this._uploadOutgoing.async(this, self.cb);

      yield this._syncFinish.async(this, self.cb);
    }
    catch (e) {
      this._log.warn("Sync failed");
      throw e;
    }
    finally {
      this._tracker.enable();
    }
  },

  _resetServer: function SyncEngine__resetServer() {
    let self = yield;
    let all = new Resource(this.engineURL);
    yield all.delete(self.cb);
  }
};
