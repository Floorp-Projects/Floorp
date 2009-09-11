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
Cu.import("resource://weave/ext/Observers.js");
Cu.import("resource://weave/ext/Sync.js");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");

Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/keys.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/collection.js");

// Singleton service, holds registered engines

Utils.lazy(this, 'Engines', EngineManagerSvc);

function EngineManagerSvc() {
  this._engines = {};
  this._log = Log4Moz.repository.getLogger("Service.Engines");
  this._log.level = Log4Moz.Level[Svc.Prefs.get(
    "log.logger.service.engines", "Debug")];
}
EngineManagerSvc.prototype = {
  get: function EngMgr_get(name) {
    // Return an array of engines if we have an array of names
    if (Utils.isArray(name)) {
      let engines = [];
      name.forEach(function(name) {
        let engine = this.get(name);
        if (engine)
          engines.push(engine);
      }, this);
      return engines;
    }

    let engine = this._engines[name];
    if (!engine)
      this._log.debug("Could not get engine: " + name);
    return engine;
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

  /**
   * Register an Engine to the service. Alternatively, give an array of engine
   * objects to register.
   *
   * @param engineObject
   *        Engine object used to get an instance of the engine
   * @return The engine object if anything failed
   */
  register: function EngMgr_register(engineObject) {
    if (Utils.isArray(engineObject))
      return engineObject.map(this.register, this);

    try {
      let name = engineObject.prototype.name;
      if (name in this._engines)
        this._log.error("Engine '" + name + "' is already registered!");
      else
        this._engines[name] = new engineObject();
    }
    catch(ex) {
      let mesg = ex.message ? ex.message : ex;
      let name = engineObject || "";
      name = name.prototype || "";
      name = name.name || "";

      let out = "Could not initialize engine '" + name + "': " + mesg;
      dump(out);
      this._log.error(out);

      return engineObject;
    }
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
  name: "engine",
  displayName: "Boring Engine",
  logName: "Engine",

  // _storeObj, and _trackerObj should to be overridden in subclasses

  _storeObj: Store,
  _trackerObj: Tracker,

  get enabled() Svc.Prefs.get("engine." + this.name, null),
  set enabled(val) Svc.Prefs.set("engine." + this.name, !!val),

  get score() this._tracker.score,

  get _store() {
    if (!this.__store)
      this.__store = new this._storeObj();
    return this.__store;
  },

  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new this._trackerObj();
    return this.__tracker;
  },

  _init: function Engine__init() {
    this._notify = Utils.notify("weave:engine:");
    this._log = Log4Moz.repository.getLogger("Engine." + this.logName);
    let level = Svc.Prefs.get("log.logger.engine." + this.name, "Debug");
    this._log.level = Log4Moz.Level[level];

    this._tracker; // initialize tracker to load previously changed IDs
    this._log.debug("Engine initialized");
  },

  sync: function Engine_sync() {
    if (!this._sync)
      throw "engine does not implement _sync method";

    let times = {};
    let wrapped = {};
    // Find functions in any point of the prototype chain
    for (let _name in this) {
      let name = _name;

      // Ignore certain constructors/functions
      if (name.search(/^_(.+Obj|notify)$/) == 0)
        continue;

      // Only track functions but skip the constructors
      if (typeof this[name] == "function") {
        times[name] = [];
        wrapped[name] = this[name];

        // Wrap the original function with a start/stop timer
        this[name] = function() {
          let start = Date.now();
          try {
            return wrapped[name].apply(this, arguments);
          }
          finally {
            times[name].push(Date.now() - start);
          }
        };
      }
    }

    try {
      this._notify("sync", this.name, this._sync)();
    }
    finally {
      // Restore original unwrapped functionality
      for (let [name, func] in Iterator(wrapped))
        this[name] = func;

      let stats = {};
      for (let [name, time] in Iterator(times)) {
        // Figure out stats on the times unless there's nothing
        let num = time.length;
        if (num == 0)
          continue;

        // Track the min/max/sum of the values
        let stat = {
          num: num,
          sum: 0
        };
        time.forEach(function(val) {
          if (val < stat.min || stat.min == null)
            stat.min = val;
          if (val > stat.max || stat.max == null)
            stat.max = val;
          stat.sum += val;
        });

        stat.avg = Number((stat.sum / num).toFixed(2));
        stats[name] = stat;
      }

      stats.toString = function() {
        let sums = [];
        for (let [name, stat] in Iterator(this))
          if (stat.sum != null)
            sums.push(name.replace(/^_/, "") + " " + stat.sum);

        return "Total (ms): " + sums.sort().join(", ");
      };

      this._log.info(stats);
    }
  },

  wipeServer: function Engine_wipeServer() {
    if (!this._wipeServer)
      throw "engine does not implement _wipeServer method";
    this._notify("wipe-server", this.name, this._wipeServer)();
  },

  /**
   * Get rid of any local meta-data
   */
  resetClient: function Engine_resetClient() {
    if (!this._resetClient)
      throw "engine does not implement _resetClient method";

    this._notify("reset-client", this.name, this._resetClient)();
  },

  _wipeClient: function Engine__wipeClient() {
    this.resetClient();
    this._log.debug("Deleting all local data");
    this._store.wipe();
  },

  wipeClient: function Engine_wipeClient() {
    this._notify("wipe-client", this.name, this._wipeClient)();
  }
};

function SyncEngine() { this._init(); }
SyncEngine.prototype = {
  __proto__: Engine.prototype,

  _recordObj: CryptoWrapper,

  _init: function _init() {
    Engine.prototype._init.call(this);
    this.loadToFetch();
  },

  get baseURL() {
    let url = Svc.Prefs.get("clusterURL");
    if (!url)
      return null;
    if (url[url.length-1] != '/')
      url += '/';
    url += "0.5/";
    return url;
  },

  get engineURL() {
    return this.baseURL + ID.get('WeaveID').username +
      '/storage/' + this.name + '/';
  },

  get cryptoMetaURL() {
    return this.baseURL + ID.get('WeaveID').username +
      '/storage/crypto/' + this.name;
  },

  get lastSync() {
    return parseFloat(Svc.Prefs.get(this.name + ".lastSync", "0"));
  },
  set lastSync(value) {
    // Reset the pref in-case it's a number instead of a string
    Svc.Prefs.reset(this.name + ".lastSync");
    // Store the value as a string to keep floating point precision
    Svc.Prefs.set(this.name + ".lastSync", value.toString());
  },
  resetLastSync: function SyncEngine_resetLastSync() {
    this._log.debug("Resetting " + this.name + " last sync time");
    Svc.Prefs.reset(this.name + ".lastSync");
    Svc.Prefs.set(this.name + ".lastSync", "0");
  },

  get toFetch() this._toFetch,
  set toFetch(val) {
    this._toFetch = val;
    Utils.jsonSave("toFetch/" + this.name, this, val);
  },

  loadToFetch: function loadToFetch() {
    // Initialize to empty if there's no file
    this._toFetch = [];
    Utils.jsonLoad("toFetch/" + this.name, this, Utils.bind2(this, function(o)
      this._toFetch = o));
  },

  // Create a new record by querying the store, and add the engine metadata
  _createRecord: function SyncEngine__createRecord(id) {
    return this._store.createRecord(id, this.cryptoMetaURL);
  },

  // Any setup that needs to happen at the beginning of each sync.
  // Makes sure crypto records and keys are all set-up
  _syncStartup: function SyncEngine__syncStartup() {
    this._log.debug("Ensuring server crypto records are there");

    let meta = CryptoMetas.get(this.cryptoMetaURL);
    if (!meta) {
      let symkey = Svc.Crypto.generateRandomKey();
      let pubkey = PubKeys.getDefaultKey();
      meta = new CryptoMeta(this.cryptoMetaURL);
      meta.generateIV();
      meta.addUnwrappedKey(pubkey, symkey);
      let res = new Resource(meta.uri);
      let resp = res.put(meta);
      if (!resp.success)
        throw resp;

      // Cache the cryto meta that we just put on the server
      CryptoMetas.set(meta.uri, meta);
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

    // Keep track of what to delete at the end of sync
    this._delete = {};
  },

  // Generate outgoing records
  _processIncoming: function SyncEngine__processIncoming() {
    this._log.debug("Downloading & applying server changes");

    // enable cache, and keep only the first few items.  Otherwise (when
    // we have more outgoing items than can fit in the cache), we will
    // keep rotating items in and out, perpetually getting cache misses
    this._store.cache.enabled = true;
    this._store.cache.fifo = false; // filo
    this._store.cache.clear();

    let newitems = new Collection(this.engineURL, this._recordObj);
    newitems.newer = this.lastSync;
    newitems.full = true;
    newitems.sort = "index";
    newitems.limit = 300;

    let count = {applied: 0, reconciled: 0};
    let handled = [];
    newitems.recordHandler = Utils.bind2(this, function(item) {
      // Remember which records were processed
      handled.push(item.id);

      try {
        item.decrypt(ID.get("WeaveCryptoID"));
        if (this._reconcile(item)) {
          count.applied++;
          this._tracker.ignoreAll = true;
          this._store.applyIncoming(item);
        } else {
          count.reconciled++;
          this._log.trace("Skipping reconciled incoming item " + item.id);
        }
      }
      catch(ex) {
        this._log.warn("Error processing record: " + Utils.exceptionStr(ex));
      }
      this._tracker.ignoreAll = false;
      Sync.sleep(0);
    });

    // Only bother getting data from the server if there's new things
    if (this.lastModified > this.lastSync) {
      let resp = newitems.get();
      if (!resp.success)
        throw resp;
    }

    // Check if we got the maximum that we requested; get the rest if so
    if (handled.length == newitems.limit) {
      let guidColl = new Collection(this.engineURL);
      guidColl.newer = this.lastSync;
      guidColl.sort = "index";

      let guids = guidColl.get();
      if (!guids.success)
        throw guids;

      // Figure out which guids weren't just fetched then remove any guids that
      // were already waiting and prepend the new ones
      let extra = Utils.arraySub(guids.obj, handled);
      if (extra.length > 0)
        this.toFetch = extra.concat(Utils.arraySub(this.toFetch, extra));
    }

    // Process any backlog of GUIDs if necessary
    if (this.toFetch.length > 0) {
      // Reuse the original query, but get rid of the restricting params
      newitems.limit = 0;
      newitems.newer = 0;

      // Get the first bunch of records and save the rest for later, but don't
      // get too many records as there's a maximum server URI length (HTTP 414)
      newitems.ids = this.toFetch.slice(0, 150);
      this.toFetch = this.toFetch.slice(150);

      // Reuse the existing record handler set earlier
      let resp = newitems.get();
      if (!resp.success)
        throw resp;
    }

    if (this.lastSync < this.lastModified)
      this.lastSync = this.lastModified;

    this._log.info(["Records:", count.applied, "applied,", count.reconciled,
      "reconciled,", this.toFetch.length, "left to fetch"].join(" "));

    // try to free some memory
    this._store.cache.clear();
    Cu.forceGC();
  },

  /**
   * Find a GUID of an item that is a duplicate of the incoming item but happens
   * to have a different GUID
   *
   * @return GUID of the similar item; falsy otherwise
   */
  _findDupe: function _findDupe(item) {
    // By default, assume there's no dupe items for the engine
  },

  _isEqual: function SyncEngine__isEqual(item) {
    let local = this._createRecord(item.id);
    if (this._log.level <= Log4Moz.Level.Trace)
      this._log.trace("Local record: " + local);
    if (item.parentid == local.parentid &&
        item.sortindex == local.sortindex &&
        item.deleted == local.deleted &&
        Utils.deepEquals(item.cleartext, local.cleartext)) {
      this._log.trace("Local record is the same");
      return true;
    } else {
      this._log.trace("Local record is different");
      return false;
    }
  },

  _deleteId: function _deleteId(id) {
    this._tracker.removeChangedID(id);

    // Remember this id to delete at the end of sync
    if (this._delete.ids == null)
      this._delete.ids = [id];
    else
      this._delete.ids.push(id);
  },

  _handleDupe: function _handleDupe(item, dupeId) {
    // The local dupe is the lower id, so pretend the incoming is for it
    if (dupeId < item.id) {
      this._deleteId(item.id);
      item.id = dupeId;
      this._tracker.changedIDs[dupeId] = true;
    }
    // The incoming item has the lower id, so change the dupe to it
    else {
      this._store.changeItemID(dupeId, item.id);
      this._deleteId(dupeId);
    }

    this._store.cache.clear(); // because parentid refs will be wrong
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
    if (this._log.level <= Log4Moz.Level.Trace)
      this._log.trace("Incoming: " + item);

    // Step 1: Check for conflicts
    //         If same as local record, do not upload
    this._log.trace("Reconcile step 1");
    if (item.id in this._tracker.changedIDs) {
      if (this._isEqual(item))
        this._tracker.removeChangedID(item.id);
      return false;
    }

    // Step 2: Check for updates
    //         If different from local record, apply server update
    this._log.trace("Reconcile step 2");
    if (this._store.itemExists(item.id))
      return !this._isEqual(item);

    // If the incoming item has been deleted, skip step 3
    this._log.trace("Reconcile step 2.5");
    if (item.deleted)
      return true;

    // Step 3: Check for similar items
    this._log.trace("Reconcile step 3");
    let dupeId = this._findDupe(item);
    if (dupeId)
      this._handleDupe(item, dupeId);

    // Apply the incoming item (now that the dupe is the right id)
    return true;
  },

  // Upload outgoing records
  _uploadOutgoing: function SyncEngine__uploadOutgoing() {
    let outnum = [i for (i in this._tracker.changedIDs)].length;
    if (outnum) {
      this._log.debug("Preparing " + outnum + " outgoing records");

      // collection we'll upload
      let up = new Collection(this.engineURL);
      let count = 0;

      // Upload what we've got so far in the collection
      let doUpload = Utils.bind2(this, function(desc) {
        this._log.info("Uploading " + desc + " of " + outnum + " records");
        let resp = up.post();
        if (!resp.success)
          throw resp;

        // Record the modified time of the upload
        let modified = resp.headers["X-Weave-Timestamp"];
        if (modified > this.lastSync)
          this.lastSync = modified;

        up.clearRecords();
      });

      // don't cache the outgoing items, we won't need them later
      this._store.cache.enabled = false;

      for (let id in this._tracker.changedIDs) {
        let out = this._createRecord(id);
        if (this._log.level <= Log4Moz.Level.Trace)
          this._log.trace("Outgoing: " + out);

        out.encrypt(ID.get("WeaveCryptoID"));
        up.pushData(out);

        // Partial upload
        if ((++count % MAX_UPLOAD_RECORDS) == 0)
          doUpload((count - MAX_UPLOAD_RECORDS) + " - " + count + " out");

        Sync.sleep(0);
      }

      // Final upload
      if (count % MAX_UPLOAD_RECORDS > 0)
        doUpload(count >= MAX_UPLOAD_RECORDS ? "last batch" : "all");

      this._store.cache.enabled = true;
    }
    this._tracker.clearChangedIDs();
  },

  // Any cleanup necessary.
  // Save the current snapshot so as to calculate changes at next sync
  _syncFinish: function SyncEngine__syncFinish() {
    this._log.trace("Finishing up sync");
    this._tracker.resetScore();

    for (let [key, val] in Iterator(this._delete)) {
      // Remove the key for future uses
      delete this._delete[key];

      // Send a delete for the property
      let coll = new Collection(this.engineURL, this._recordObj);
      coll[key] = val;
      coll.delete();
    }
  },

  _sync: function SyncEngine__sync() {
    try {
      this._syncStartup();
      Observers.notify("weave:engine:sync:status", "process-incoming");
      this._processIncoming();
      Observers.notify("weave:engine:sync:status", "upload-outgoing");
      this._uploadOutgoing();
      this._syncFinish();
    }
    catch (e) {
      this._log.warn("Sync failed");
      throw e;
    }
  },

  _wipeServer: function SyncEngine__wipeServer() {
    new Resource(this.engineURL).delete();
    new Resource(this.cryptoMetaURL).delete();
  },

  _resetClient: function SyncEngine__resetClient() {
    this.resetLastSync();
    this.toFetch = [];
  }
};
