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

const EXPORTED_SYMBOLS = ['Engines', 'Engine', 'SyncEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/resource.js");
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

function Engine() { this._init(); }
Engine.prototype = {
  _notify: Wrap.notify,

  name: "engine",
  displayName: "Boring Engine",
  logName: "Engine",

  // _storeObj, and _trackerObj should to be overridden in subclasses

  _storeObj: Store,
  _trackerObj: Tracker,

  get enabled() Utils.prefs.getBoolPref("engine." + this.name),
  get score() this._tracker.score,

  get _os() {
    let os = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
    this.__defineGetter__("_os", function() os);
    return os;
  },

  get _store() {
    let store = new this._storeObj();
    this.__defineGetter__("_store", function() store);
    return store;
  },

  get _tracker() {
    let tracker = new this._trackerObj();
    this.__defineGetter__("_tracker", function() tracker);
    return tracker;
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

  sync: function Engine_sync(onComplete) {
    if (!this._sync)
      throw "engine does not implement _sync method";
    this._notify("sync", "", this._sync).async(this, onComplete);
  },

  wipeServer: function Engimne_wipeServer(onComplete) {
    if (!this._wipeServer)
      throw "engine does not implement _wipeServer method";
    this._notify("wipe-server", "", this._wipeServer).async(this, onComplete);
  },

  _wipeClient: function Engine__wipeClient() {
    let self = yield;
    this._log.debug("Deleting all local data");
    this._store.wipe();
  },
  wipeClient: function Engine_wipeClient(onComplete) {
    this._notify("wipe-client", "", this._wipeClient).async(this, onComplete);
  }
};

function SyncEngine() { this._init(); }
SyncEngine.prototype = {
  __proto__: Engine.prototype,

  _recordObj: CryptoWrapper,

  get _memory() {
    let mem = Cc["@mozilla.org/xpcom/memory-service;1"].getService(Ci.nsIMemory);
    this.__defineGetter__("_memory", function() mem);
    return mem;
  },

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

  _lowMemCheck: function SyncEngine__lowMemCheck() {
    if (this._memory.isLowMemory()) {
      this._log.warn("Low memory, forcing GC");
      Cu.forceGC();
      if (this._memory.isLowMemory()) {
        this._log.warn("Low memory, aborting sync!");
        throw "Low memory";
      }
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

    // first sync special case: upload all items
    // NOTE: we use a backdoor (of sorts) to the tracker so it
    // won't save to disk this list over and over
    if (!this.lastSync) {
      this._log.info("First sync, uploading all items");
      this._tracker.clearChangedIDs();
      [i for (i in this._store.getAllIDs())]
        .forEach(function(id) this._tracker.changedIDs[id] = true, this);
    }

    let outnum = [i for (i in this._tracker.changedIDs)].length;
    this._log.info(outnum + " outgoing items pre-reconciliation");

    this._tracker.disable(); // FIXME: need finer-grained ignoring
  },

  // Generate outgoing records
  _processIncoming: function SyncEngine__processIncoming() {
    let self = yield;

    this._log.debug("Downloading & applying server changes");

    // enable cache, and keep only the first few items.  Otherwise (when
    // we have more outgoing items than can fit in the cache), we will
    // keep rotating items in and out, perpetually getting cache misses
    this._store.cache.enabled = true;
    this._store.cache.fifo = false; // filo
    this._store.cache.clear();

    let newitems = new Collection(this.engineURL, this._recordObj);
    newitems.modified = this.lastSync;
    newitems.full = true;
    newitems.sort = "depthindex";
    yield newitems.get(self.cb);

    let item;
    this._lastSyncTmp = 0;
    while ((item = yield newitems.iter.next(self.cb))) {
      this._lowMemCheck();
      yield item.decrypt(self.cb, ID.get('WeaveCryptoID').password);
      if (yield this._reconcile.async(this, self.cb, item))
        yield this._applyIncoming.async(this, self.cb, item);
      else {
        this._log.trace("Skipping reconciled incoming item " + item.id);
        if (this._lastSyncTmp < item.modified)
          this._lastSyncTmp = item.modified;
      }
    }
    if (this.lastSync < this._lastSyncTmp)
        this.lastSync = this._lastSyncTmp;

    // try to free some memory
    this._store.cache.clear();
    Cu.forceGC();
  },

  _isEqual: function SyncEngine__isEqual(item) {
    let local = this._createRecord(item.id);
    this._log.trace("Local record: \n" + local);
    if (item.parentid == local.parentid &&
        item.sortindex == local.sortindex &&
        Utils.deepEquals(item.cleartext, local.cleartext)) {
      this._log.trace("Local record is the same");
      return true;
    } else {
      this._log.trace("Local record is different");
      return false;
    }
  },

  // Reconciliation has three steps:
  // 1) Check for the same item (same ID) on both the incoming and outgoing
  //    queues.  This means the same item was modified on this profile and
  //    another at the same time.  In this case, this client wins (which really
  //    means, the last profile you sync wins).
  // 2) Check if the incoming item's ID exists locally.  In that case it's an
  //    update and we should not try a similarity check (step 3)
  // 3) Check if any incoming & outgoing items are actually the same, even
  //    though they have different IDs.  This happens when the same item is
  //    added on two different machines at the same time.  It's also the common
  //    case when syncing for the first time two machines that already have the
  //    same bookmarks.  In this case we change the IDs to match.
  _reconcile: function SyncEngine__reconcile(item) {
    let self = yield;
    let ret = true;

    // Step 1: Check for conflicts
    //         If same as local record, do not upload
    if (item.id in this._tracker.changedIDs) {
      if (this._isEqual(item))
        this._tracker.removeChangedID(item.id);
      self.done(false);
      return;
    }

    // Step 2: Check for updates
    //         If different from local record, apply server update
    if (this._store.itemExists(item.id)) {
      self.done(!this._isEqual(item));
      return;
    }

    // Step 3: Check for similar items
    for (let id in this._tracker.changedIDs) {
      let out = this._createRecord(id);
      if (this._recordLike(item, out)) {
        this._store.changeItemID(id, item.id);
        this._tracker.removeChangedID(item.id);
        this._store.cache.clear(); // because parentid refs will be wrong
        self.done(false);
        return;
      }
    }
    self.done(true);
  },

  // Apply incoming records
  _applyIncoming: function SyncEngine__applyIncoming(item) {
    let self = yield;
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

    let outnum = [i for (i in this._tracker.changedIDs)].length;
    this._log.debug("Preparing " + outnum + " outgoing records");
    if (outnum) {
      // collection we'll upload
      let up = new Collection(this.engineURL);
      let meta = {};

      // don't cache the outgoing items, we won't need them later
      this._store.cache.enabled = false;

      for (let id in this._tracker.changedIDs) {
        let out = this._createRecord(id);
        this._log.trace("Outgoing:\n" + out);
        if (out.cleartext) // skip deleted records
          this._store.createMetaRecords(out.id, meta);
        yield out.encrypt(self.cb, ID.get('WeaveCryptoID').password);
        yield up.pushRecord(self.cb, out);
      }

      this._store.cache.enabled = true;

      // now add short depth-and-index-only records, except the ones we're
      // sending as full records
      let count = 0;
      for each (let obj in meta) {
          if (!(obj.id in this._tracker.changedIDs)) {
            up.pushLiteral(obj);
            count++;
          }
      }

      this._log.debug("Uploading " + outnum + " records + " + count + " index/depth records)");
      // do the upload
      yield up.post(self.cb);

      // save last modified date
      let mod = up.data.modified;
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
      yield this._processIncoming.async(this, self.cb);
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

  _wipeServer: function SyncEngine__wipeServer() {
    let self = yield;
    let all = new Resource(this.engineURL);
    yield all.delete(self.cb);
    let crypto = new Resource(this.cryptoMetaURL);
    yield crypto.delete(self.cb);
  }
};
