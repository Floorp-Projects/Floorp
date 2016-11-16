/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "EngineManager",
  "Engine",
  "SyncEngine",
  "Tracker",
  "Store",
  "Changeset"
];

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://services-common/async.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

/*
 * Trackers are associated with a single engine and deal with
 * listening for changes to their particular data type.
 *
 * There are two things they keep track of:
 * 1) A score, indicating how urgently the engine wants to sync
 * 2) A list of IDs for all the changed items that need to be synced
 * and updating their 'score', indicating how urgently they
 * want to sync.
 *
 */
this.Tracker = function Tracker(name, engine) {
  if (!engine) {
    throw new Error("Tracker must be associated with an Engine instance.");
  }

  name = name || "Unnamed";
  this.name = this.file = name.toLowerCase();
  this.engine = engine;

  this._log = Log.repository.getLogger("Sync.Tracker." + name);
  let level = Svc.Prefs.get("log.logger.engine." + this.name, "Debug");
  this._log.level = Log.Level[level];

  this._score = 0;
  this._ignored = [];
  this.ignoreAll = false;
  this.changedIDs = {};
  this.loadChangedIDs();

  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);

  Svc.Prefs.observe("engine." + this.engine.prefName, this);
};

Tracker.prototype = {
  /*
   * Score can be called as often as desired to decide which engines to sync
   *
   * Valid values for score:
   * -1: Do not sync unless the user specifically requests it (almost disabled)
   * 0: Nothing has changed
   * 100: Please sync me ASAP!
   *
   * Setting it to other values should (but doesn't currently) throw an exception
   */
  get score() {
    return this._score;
  },

  set score(value) {
    this._score = value;
    Observers.notify("weave:engine:score:updated", this.name);
  },

  // Should be called by service everytime a sync has been done for an engine
  resetScore: function () {
    this._score = 0;
  },

  persistChangedIDs: true,

  /**
   * Persist changedIDs to disk at a later date.
   * Optionally pass a callback to be invoked when the write has occurred.
   */
  saveChangedIDs: function (cb) {
    if (!this.persistChangedIDs) {
      this._log.debug("Not saving changedIDs.");
      return;
    }
    Utils.namedTimer(function () {
      this._log.debug("Saving changed IDs to " + this.file);
      Utils.jsonSave("changes/" + this.file, this, this.changedIDs, cb);
    }, 1000, this, "_lazySave");
  },

  loadChangedIDs: function (cb) {
    Utils.jsonLoad("changes/" + this.file, this, function(json) {
      if (json && (typeof(json) == "object")) {
        this.changedIDs = json;
      } else if (json !== null) {
        this._log.warn("Changed IDs file " + this.file + " contains non-object value.");
        json = null;
      }
      if (cb) {
        cb.call(this, json);
      }
    });
  },

  // ignore/unignore specific IDs.  Useful for ignoring items that are
  // being processed, or that shouldn't be synced.
  // But note: not persisted to disk

  ignoreID: function (id) {
    this.unignoreID(id);
    this._ignored.push(id);
  },

  unignoreID: function (id) {
    let index = this._ignored.indexOf(id);
    if (index != -1)
      this._ignored.splice(index, 1);
  },

  _saveChangedID(id, when) {
    this._log.trace(`Adding changed ID: ${id}, ${JSON.stringify(when)}`);
    this.changedIDs[id] = when;
    this.saveChangedIDs(this.onSavedChangedIDs);
  },

  addChangedID: function (id, when) {
    if (!id) {
      this._log.warn("Attempted to add undefined ID to tracker");
      return false;
    }

    if (this.ignoreAll || this._ignored.includes(id)) {
      return false;
    }

    // Default to the current time in seconds if no time is provided.
    if (when == null) {
      when = this._now();
    }

    // Add/update the entry if we have a newer time.
    if ((this.changedIDs[id] || -Infinity) < when) {
      this._saveChangedID(id, when);
    }

    return true;
  },

  removeChangedID: function (id) {
    if (!id) {
      this._log.warn("Attempted to remove undefined ID to tracker");
      return false;
    }
    if (this.ignoreAll || this._ignored.includes(id)) {
      return false;
    }
    if (this.changedIDs[id] != null) {
      this._log.trace("Removing changed ID " + id);
      delete this.changedIDs[id];
      this.saveChangedIDs();
    }
    return true;
  },

  clearChangedIDs: function () {
    this._log.trace("Clearing changed ID list");
    this.changedIDs = {};
    this.saveChangedIDs();
  },

  _now() {
    return Date.now() / 1000;
  },

  _isTracking: false,

  // Override these in your subclasses.
  startTracking: function () {
  },

  stopTracking: function () {
  },

  engineIsEnabled: function () {
    if (!this.engine) {
      // Can't tell -- we must be running in a test!
      return true;
    }
    return this.engine.enabled;
  },

  onEngineEnabledChanged: function (engineEnabled) {
    if (engineEnabled == this._isTracking) {
      return;
    }

    if (engineEnabled) {
      this.startTracking();
      this._isTracking = true;
    } else {
      this.stopTracking();
      this._isTracking = false;
      this.clearChangedIDs();
    }
  },

  observe: function (subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this.engineIsEnabled()) {
          return;
        }
        this._log.trace("Got start-tracking.");
        if (!this._isTracking) {
          this.startTracking();
          this._isTracking = true;
        }
        return;
      case "weave:engine:stop-tracking":
        this._log.trace("Got stop-tracking.");
        if (this._isTracking) {
          this.stopTracking();
          this._isTracking = false;
        }
        return;
      case "nsPref:changed":
        if (data == PREFS_BRANCH + "engine." + this.engine.prefName) {
          this.onEngineEnabledChanged(this.engine.enabled);
        }
        return;
    }
  }
};



/**
 * The Store serves as the interface between Sync and stored data.
 *
 * The name "store" is slightly a misnomer because it doesn't actually "store"
 * anything. Instead, it serves as a gateway to something that actually does
 * the "storing."
 *
 * The store is responsible for record management inside an engine. It tells
 * Sync what items are available for Sync, converts items to and from Sync's
 * record format, and applies records from Sync into changes on the underlying
 * store.
 *
 * Store implementations require a number of functions to be implemented. These
 * are all documented below.
 *
 * For stores that deal with many records or which have expensive store access
 * routines, it is highly recommended to implement a custom applyIncomingBatch
 * and/or applyIncoming function on top of the basic APIs.
 */

this.Store = function Store(name, engine) {
  if (!engine) {
    throw new Error("Store must be associated with an Engine instance.");
  }

  name = name || "Unnamed";
  this.name = name.toLowerCase();
  this.engine = engine;

  this._log = Log.repository.getLogger("Sync.Store." + name);
  let level = Svc.Prefs.get("log.logger.engine." + this.name, "Debug");
  this._log.level = Log.Level[level];

  XPCOMUtils.defineLazyGetter(this, "_timer", function() {
    return Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  });
}
Store.prototype = {

  _sleep: function _sleep(delay) {
    let cb = Async.makeSyncCallback();
    this._timer.initWithCallback(cb, delay, Ci.nsITimer.TYPE_ONE_SHOT);
    Async.waitForSyncCallback(cb);
  },

  /**
   * Apply multiple incoming records against the store.
   *
   * This is called with a set of incoming records to process. The function
   * should look at each record, reconcile with the current local state, and
   * make the local changes required to bring its state in alignment with the
   * record.
   *
   * The default implementation simply iterates over all records and calls
   * applyIncoming(). Store implementations may overwrite this function
   * if desired.
   *
   * @param  records Array of records to apply
   * @return Array of record IDs which did not apply cleanly
   */
  applyIncomingBatch: function (records) {
    let failed = [];
    for (let record of records) {
      try {
        this.applyIncoming(record);
      } catch (ex) {
        if (ex.code == Engine.prototype.eEngineAbortApplyIncoming) {
          // This kind of exception should have a 'cause' attribute, which is an
          // originating exception.
          // ex.cause will carry its stack with it when rethrown.
          throw ex.cause;
        }
        if (Async.isShutdownException(ex)) {
          throw ex;
        }
        this._log.warn("Failed to apply incoming record " + record.id, ex);
        this.engine._noteApplyFailure();
        failed.push(record.id);
      }
    };
    return failed;
  },

  /**
   * Apply a single record against the store.
   *
   * This takes a single record and makes the local changes required so the
   * local state matches what's in the record.
   *
   * The default implementation calls one of remove(), create(), or update()
   * depending on the state obtained from the store itself. Store
   * implementations may overwrite this function if desired.
   *
   * @param record
   *        Record to apply
   */
  applyIncoming: function (record) {
    if (record.deleted)
      this.remove(record);
    else if (!this.itemExists(record.id))
      this.create(record);
    else
      this.update(record);
  },

  // override these in derived objects

  /**
   * Create an item in the store from a record.
   *
   * This is called by the default implementation of applyIncoming(). If using
   * applyIncomingBatch(), this won't be called unless your store calls it.
   *
   * @param record
   *        The store record to create an item from
   */
  create: function (record) {
    throw "override create in a subclass";
  },

  /**
   * Remove an item in the store from a record.
   *
   * This is called by the default implementation of applyIncoming(). If using
   * applyIncomingBatch(), this won't be called unless your store calls it.
   *
   * @param record
   *        The store record to delete an item from
   */
  remove: function (record) {
    throw "override remove in a subclass";
  },

  /**
   * Update an item from a record.
   *
   * This is called by the default implementation of applyIncoming(). If using
   * applyIncomingBatch(), this won't be called unless your store calls it.
   *
   * @param record
   *        The record to use to update an item from
   */
  update: function (record) {
    throw "override update in a subclass";
  },

  /**
   * Determine whether a record with the specified ID exists.
   *
   * Takes a string record ID and returns a booleans saying whether the record
   * exists.
   *
   * @param  id
   *         string record ID
   * @return boolean indicating whether record exists locally
   */
  itemExists: function (id) {
    throw "override itemExists in a subclass";
  },

  /**
   * Create a record from the specified ID.
   *
   * If the ID is known, the record should be populated with metadata from
   * the store. If the ID is not known, the record should be created with the
   * delete field set to true.
   *
   * @param  id
   *         string record ID
   * @param  collection
   *         Collection to add record to. This is typically passed into the
   *         constructor for the newly-created record.
   * @return record type for this engine
   */
  createRecord: function (id, collection) {
    throw "override createRecord in a subclass";
  },

  /**
   * Change the ID of a record.
   *
   * @param  oldID
   *         string old/current record ID
   * @param  newID
   *         string new record ID
   */
  changeItemID: function (oldID, newID) {
    throw "override changeItemID in a subclass";
  },

  /**
   * Obtain the set of all known record IDs.
   *
   * @return Object with ID strings as keys and values of true. The values
   *         are ignored.
   */
  getAllIDs: function () {
    throw "override getAllIDs in a subclass";
  },

  /**
   * Wipe all data in the store.
   *
   * This function is called during remote wipes or when replacing local data
   * with remote data.
   *
   * This function should delete all local data that the store is managing. It
   * can be thought of as clearing out all state and restoring the "new
   * browser" state.
   */
  wipe: function () {
    throw "override wipe in a subclass";
  }
};

this.EngineManager = function EngineManager(service) {
  this.service = service;

  this._engines = {};

  // This will be populated by Service on startup.
  this._declined = new Set();
  this._log = Log.repository.getLogger("Sync.EngineManager");
  this._log.level = Log.Level[Svc.Prefs.get("log.logger.service.engines", "Debug")];
}
EngineManager.prototype = {
  get: function (name) {
    // Return an array of engines if we have an array of names
    if (Array.isArray(name)) {
      let engines = [];
      name.forEach(function(name) {
        let engine = this.get(name);
        if (engine) {
          engines.push(engine);
        }
      }, this);
      return engines;
    }

    let engine = this._engines[name];
    if (!engine) {
      this._log.debug("Could not get engine: " + name);
      if (Object.keys) {
        this._log.debug("Engines are: " + JSON.stringify(Object.keys(this._engines)));
      }
    }
    return engine;
  },

  getAll: function () {
    let engines = [];
    for (let [, engine] of Object.entries(this._engines)) {
      engines.push(engine);
    }
    return engines;
  },

  /**
   * N.B., does not pay attention to the declined list.
   */
  getEnabled: function () {
    return this.getAll()
               .filter((engine) => engine.enabled)
               .sort((a, b) => a.syncPriority - b.syncPriority);
  },

  get enabledEngineNames() {
    return this.getEnabled().map(e => e.name);
  },

  persistDeclined: function () {
    Svc.Prefs.set("declinedEngines", [...this._declined].join(","));
  },

  /**
   * Returns an array.
   */
  getDeclined: function () {
    return [...this._declined];
  },

  setDeclined: function (engines) {
    this._declined = new Set(engines);
    this.persistDeclined();
  },

  isDeclined: function (engineName) {
    return this._declined.has(engineName);
  },

  /**
   * Accepts a Set or an array.
   */
  decline: function (engines) {
    for (let e of engines) {
      this._declined.add(e);
    }
    this.persistDeclined();
  },

  undecline: function (engines) {
    for (let e of engines) {
      this._declined.delete(e);
    }
    this.persistDeclined();
  },

  /**
   * Mark any non-enabled engines as declined.
   *
   * This is useful after initial customization during setup.
   */
  declineDisabled: function () {
    for (let e of this.getAll()) {
      if (!e.enabled) {
        this._log.debug("Declining disabled engine " + e.name);
        this._declined.add(e.name);
      }
    }
    this.persistDeclined();
  },

  /**
   * Register an Engine to the service. Alternatively, give an array of engine
   * objects to register.
   *
   * @param engineObject
   *        Engine object used to get an instance of the engine
   * @return The engine object if anything failed
   */
  register: function (engineObject) {
    if (Array.isArray(engineObject)) {
      return engineObject.map(this.register, this);
    }

    try {
      let engine = new engineObject(this.service);
      let name = engine.name;
      if (name in this._engines) {
        this._log.error("Engine '" + name + "' is already registered!");
      } else {
        this._engines[name] = engine;
      }
    } catch (ex) {
      let name = engineObject || "";
      name = name.prototype || "";
      name = name.name || "";

      this._log.error(`Could not initialize engine ${name}`, ex);
      return engineObject;
    }
  },

  unregister: function (val) {
    let name = val;
    if (val instanceof Engine) {
      name = val.name;
    }
    delete this._engines[name];
  },

  clear: function () {
    for (let name in this._engines) {
      delete this._engines[name];
    }
  },
};

this.Engine = function Engine(name, service) {
  if (!service) {
    throw new Error("Engine must be associated with a Service instance.");
  }

  this.Name = name || "Unnamed";
  this.name = name.toLowerCase();
  this.service = service;

  this._notify = Utils.notify("weave:engine:");
  this._log = Log.repository.getLogger("Sync.Engine." + this.Name);
  let level = Svc.Prefs.get("log.logger.engine." + this.name, "Debug");
  this._log.level = Log.Level[level];

  this._tracker; // initialize tracker to load previously changed IDs
  this._log.debug("Engine initialized");
}
Engine.prototype = {
  // _storeObj, and _trackerObj should to be overridden in subclasses
  _storeObj: Store,
  _trackerObj: Tracker,

  // Local 'constant'.
  // Signal to the engine that processing further records is pointless.
  eEngineAbortApplyIncoming: "error.engine.abort.applyincoming",

  // Should we keep syncing if we find a record that cannot be uploaded (ever)?
  // If this is false, we'll throw, otherwise, we'll ignore the record and
  // continue. This currently can only happen due to the record being larger
  // than the record upload limit.
  allowSkippedRecord: false,

  get prefName() {
    return this.name;
  },

  get enabled() {
    return Svc.Prefs.get("engine." + this.prefName, false);
  },

  set enabled(val) {
    Svc.Prefs.set("engine." + this.prefName, !!val);
  },

  get score() {
    return this._tracker.score;
  },

  get _store() {
    let store = new this._storeObj(this.Name, this);
    this.__defineGetter__("_store", () => store);
    return store;
  },

  get _tracker() {
    let tracker = new this._trackerObj(this.Name, this);
    this.__defineGetter__("_tracker", () => tracker);
    return tracker;
  },

  sync: function () {
    if (!this.enabled) {
      return;
    }

    if (!this._sync) {
      throw "engine does not implement _sync method";
    }

    this._notify("sync", this.name, this._sync)();
  },

  /**
   * Get rid of any local meta-data.
   */
  resetClient: function () {
    if (!this._resetClient) {
      throw "engine does not implement _resetClient method";
    }

    this._notify("reset-client", this.name, this._resetClient)();
  },

  _wipeClient: function () {
    this.resetClient();
    this._log.debug("Deleting all local data");
    this._tracker.ignoreAll = true;
    this._store.wipe();
    this._tracker.ignoreAll = false;
    this._tracker.clearChangedIDs();
  },

  wipeClient: function () {
    this._notify("wipe-client", this.name, this._wipeClient)();
  },

  /**
   * If one exists, initialize and return a validator for this engine (which
   * must have a `validate(engine)` method that returns a promise to an object
   * with a getSummary method). Otherwise return null.
   */
  getValidator: function () {
    return null;
  }
};

this.SyncEngine = function SyncEngine(name, service) {
  Engine.call(this, name || "SyncEngine", service);

  this.loadToFetch();
  this.loadPreviousFailed();
}

// Enumeration to define approaches to handling bad records.
// Attached to the constructor to allow use as a kind of static enumeration.
SyncEngine.kRecoveryStrategy = {
  ignore: "ignore",
  retry:  "retry",
  error:  "error"
};

SyncEngine.prototype = {
  __proto__: Engine.prototype,
  _recordObj: CryptoWrapper,
  version: 1,

  // Which sortindex to use when retrieving records for this engine.
  _defaultSort: undefined,

  // A relative priority to use when computing an order
  // for engines to be synced. Higher-priority engines
  // (lower numbers) are synced first.
  // It is recommended that a unique value be used for each engine,
  // in order to guarantee a stable sequence.
  syncPriority: 0,

  // How many records to pull in a single sync. This is primarily to avoid very
  // long first syncs against profiles with many history records.
  downloadLimit: null,

  // How many records to pull at one time when specifying IDs. This is to avoid
  // URI length limitations.
  guidFetchBatchSize: DEFAULT_GUID_FETCH_BATCH_SIZE,
  mobileGUIDFetchBatchSize: DEFAULT_MOBILE_GUID_FETCH_BATCH_SIZE,

  // How many records to process in a single batch.
  applyIncomingBatchSize: DEFAULT_STORE_BATCH_SIZE,

  get storageURL() {
    return this.service.storageURL;
  },

  get engineURL() {
    return this.storageURL + this.name;
  },

  get cryptoKeysURL() {
    return this.storageURL + "crypto/keys";
  },

  get metaURL() {
    return this.storageURL + "meta/global";
  },

  get syncID() {
    // Generate a random syncID if we don't have one
    let syncID = Svc.Prefs.get(this.name + ".syncID", "");
    return syncID == "" ? this.syncID = Utils.makeGUID() : syncID;
  },
  set syncID(value) {
    Svc.Prefs.set(this.name + ".syncID", value);
  },

  /*
   * lastSync is a timestamp in server time.
   */
  get lastSync() {
    return parseFloat(Svc.Prefs.get(this.name + ".lastSync", "0"));
  },
  set lastSync(value) {
    // Reset the pref in-case it's a number instead of a string
    Svc.Prefs.reset(this.name + ".lastSync");
    // Store the value as a string to keep floating point precision
    Svc.Prefs.set(this.name + ".lastSync", value.toString());
  },
  resetLastSync: function () {
    this._log.debug("Resetting " + this.name + " last sync time");
    Svc.Prefs.reset(this.name + ".lastSync");
    Svc.Prefs.set(this.name + ".lastSync", "0");
    this.lastSyncLocal = 0;
  },

  get toFetch() {
    return this._toFetch;
  },
  set toFetch(val) {
    let cb = (error) => {
      if (error) {
        this._log.error("Failed to read JSON records to fetch", error);
      }
    }
    // Coerce the array to a string for more efficient comparison.
    if (val + "" == this._toFetch) {
      return;
    }
    this._toFetch = val;
    Utils.namedTimer(function () {
      Utils.jsonSave("toFetch/" + this.name, this, val, cb);
    }, 0, this, "_toFetchDelay");
  },

  loadToFetch: function () {
    // Initialize to empty if there's no file.
    this._toFetch = [];
    Utils.jsonLoad("toFetch/" + this.name, this, function(toFetch) {
      if (toFetch) {
        this._toFetch = toFetch;
      }
    });
  },

  get previousFailed() {
    return this._previousFailed;
  },
  set previousFailed(val) {
    let cb = (error) => {
      if (error) {
        this._log.error("Failed to set previousFailed", error);
      } else {
        this._log.debug("Successfully wrote previousFailed.");
      }
    }
    // Coerce the array to a string for more efficient comparison.
    if (val + "" == this._previousFailed) {
      return;
    }
    this._previousFailed = val;
    Utils.namedTimer(function () {
      Utils.jsonSave("failed/" + this.name, this, val, cb);
    }, 0, this, "_previousFailedDelay");
  },

  loadPreviousFailed: function () {
    // Initialize to empty if there's no file
    this._previousFailed = [];
    Utils.jsonLoad("failed/" + this.name, this, function(previousFailed) {
      if (previousFailed) {
        this._previousFailed = previousFailed;
      }
    });
  },

  /*
   * lastSyncLocal is a timestamp in local time.
   */
  get lastSyncLocal() {
    return parseInt(Svc.Prefs.get(this.name + ".lastSyncLocal", "0"), 10);
  },
  set lastSyncLocal(value) {
    // Store as a string because pref can only store C longs as numbers.
    Svc.Prefs.set(this.name + ".lastSyncLocal", value.toString());
  },

  /*
   * Returns a changeset for this sync. Engine implementations can override this
   * method to bypass the tracker for certain or all changed items.
   */
  getChangedIDs: function () {
    return this._tracker.changedIDs;
  },

  // Create a new record using the store and add in crypto fields.
  _createRecord: function (id) {
    let record = this._store.createRecord(id, this.name);
    record.id = id;
    record.collection = this.name;
    return record;
  },

  // Any setup that needs to happen at the beginning of each sync.
  _syncStartup: function () {

    // Determine if we need to wipe on outdated versions
    let metaGlobal = this.service.recordManager.get(this.metaURL);
    let engines = metaGlobal.payload.engines || {};
    let engineData = engines[this.name] || {};

    let needsWipe = false;

    // Assume missing versions are 0 and wipe the server
    if ((engineData.version || 0) < this.version) {
      this._log.debug("Old engine data: " + [engineData.version, this.version]);

      // Prepare to clear the server and upload everything
      needsWipe = true;
      this.syncID = "";

      // Set the newer version and newly generated syncID
      engineData.version = this.version;
      engineData.syncID = this.syncID;

      // Put the new data back into meta/global and mark for upload
      engines[this.name] = engineData;
      metaGlobal.payload.engines = engines;
      metaGlobal.changed = true;
    }
    // Don't sync this engine if the server has newer data
    else if (engineData.version > this.version) {
      let error = new String("New data: " + [engineData.version, this.version]);
      error.failureCode = VERSION_OUT_OF_DATE;
      throw error;
    }
    // Changes to syncID mean we'll need to upload everything
    else if (engineData.syncID != this.syncID) {
      this._log.debug("Engine syncIDs: " + [engineData.syncID, this.syncID]);
      this.syncID = engineData.syncID;
      this._resetClient();
    };

    // Delete any existing data and reupload on bad version or missing meta.
    // No crypto component here...? We could regenerate per-collection keys...
    if (needsWipe) {
      this.wipeServer();
    }

    // Save objects that need to be uploaded in this._modified. We also save
    // the timestamp of this fetch in this.lastSyncLocal. As we successfully
    // upload objects we remove them from this._modified. If an error occurs
    // or any objects fail to upload, they will remain in this._modified. At
    // the end of a sync, or after an error, we add all objects remaining in
    // this._modified to the tracker.
    this.lastSyncLocal = Date.now();
    if (this.lastSync) {
      this._modified = this.pullNewChanges();
    } else {
      this._log.debug("First sync, uploading all items");
      this._modified = this.pullAllChanges();
    }
    // Clear the tracker now. If the sync fails we'll add the ones we failed
    // to upload back.
    this._tracker.clearChangedIDs();

    this._log.info(this._modified.count() +
                   " outgoing items pre-reconciliation");

    // Keep track of what to delete at the end of sync
    this._delete = {};
  },

  /**
   * A tiny abstraction to make it easier to test incoming record
   * application.
   */
  itemSource: function () {
    return new Collection(this.engineURL, this._recordObj, this.service);
  },

  /**
   * Process incoming records.
   * In the most awful and untestable way possible.
   * This now accepts something that makes testing vaguely less impossible.
   */
  _processIncoming: function (newitems) {
    this._log.trace("Downloading & applying server changes");

    // Figure out how many total items to fetch this sync; do less on mobile.
    let batchSize = this.downloadLimit || Infinity;
    let isMobile = (Svc.Prefs.get("client.type") == "mobile");

    if (!newitems) {
      newitems = this.itemSource();
    }

    if (this._defaultSort) {
      newitems.sort = this._defaultSort;
    }

    if (isMobile) {
      batchSize = MOBILE_BATCH_SIZE;
    }
    newitems.newer = this.lastSync;
    newitems.full  = true;
    newitems.limit = batchSize;

    // applied    => number of items that should be applied.
    // failed     => number of items that failed in this sync.
    // newFailed  => number of items that failed for the first time in this sync.
    // reconciled => number of items that were reconciled.
    let count = {applied: 0, failed: 0, newFailed: 0, reconciled: 0};
    let handled = [];
    let applyBatch = [];
    let failed = [];
    let failedInPreviousSync = this.previousFailed;
    let fetchBatch = Utils.arrayUnion(this.toFetch, failedInPreviousSync);
    // Reset previousFailed for each sync since previously failed items may not fail again.
    this.previousFailed = [];

    // Used (via exceptions) to allow the record handler/reconciliation/etc.
    // methods to signal that they would like processing of incoming records to
    // cease.
    let aborting = undefined;

    function doApplyBatch() {
      this._tracker.ignoreAll = true;
      try {
        failed = failed.concat(this._store.applyIncomingBatch(applyBatch));
      } catch (ex) {
        if (Async.isShutdownException(ex)) {
          throw ex;
        }
        // Catch any error that escapes from applyIncomingBatch. At present
        // those will all be abort events.
        this._log.warn("Got exception, aborting processIncoming", ex);
        aborting = ex;
      }
      this._tracker.ignoreAll = false;
      applyBatch = [];
    }

    function doApplyBatchAndPersistFailed() {
      // Apply remaining batch.
      if (applyBatch.length) {
        doApplyBatch.call(this);
      }
      // Persist failed items so we refetch them.
      if (failed.length) {
        this.previousFailed = Utils.arrayUnion(failed, this.previousFailed);
        count.failed += failed.length;
        this._log.debug("Records that failed to apply: " + failed);
        failed = [];
      }
    }

    let key = this.service.collectionKeys.keyForCollection(this.name);

    // Not binding this method to 'this' for performance reasons. It gets
    // called for every incoming record.
    let self = this;

    newitems.recordHandler = function(item) {
      if (aborting) {
        return;
      }

      // Grab a later last modified if possible
      if (self.lastModified == null || item.modified > self.lastModified)
        self.lastModified = item.modified;

      // Track the collection for the WBO.
      item.collection = self.name;

      // Remember which records were processed
      handled.push(item.id);

      try {
        try {
          item.decrypt(key);
        } catch (ex) {
          if (!Utils.isHMACMismatch(ex)) {
            throw ex;
          }
          let strategy = self.handleHMACMismatch(item, true);
          if (strategy == SyncEngine.kRecoveryStrategy.retry) {
            // You only get one retry.
            try {
              // Try decrypting again, typically because we've got new keys.
              self._log.info("Trying decrypt again...");
              key = self.service.collectionKeys.keyForCollection(self.name);
              item.decrypt(key);
              strategy = null;
            } catch (ex) {
              if (!Utils.isHMACMismatch(ex)) {
                throw ex;
              }
              strategy = self.handleHMACMismatch(item, false);
            }
          }

          switch (strategy) {
            case null:
              // Retry succeeded! No further handling.
              break;
            case SyncEngine.kRecoveryStrategy.retry:
              self._log.debug("Ignoring second retry suggestion.");
              // Fall through to error case.
            case SyncEngine.kRecoveryStrategy.error:
              self._log.warn("Error decrypting record", ex);
              self._noteApplyFailure();
              failed.push(item.id);
              return;
            case SyncEngine.kRecoveryStrategy.ignore:
              self._log.debug("Ignoring record " + item.id +
                              " with bad HMAC: already handled.");
              return;
          }
        }
      } catch (ex) {
        if (Async.isShutdownException(ex)) {
          throw ex;
        }
        self._log.warn("Error decrypting record", ex);
        self._noteApplyFailure();
        failed.push(item.id);
        return;
      }

      if (self._shouldDeleteRemotely(item)) {
        self._log.trace("Deleting item from server without applying", item);
        self._deleteId(item.id);
        return;
      }

      let shouldApply;
      try {
        shouldApply = self._reconcile(item);
      } catch (ex) {
        if (ex.code == Engine.prototype.eEngineAbortApplyIncoming) {
          self._log.warn("Reconciliation failed: aborting incoming processing.");
          self._noteApplyFailure();
          failed.push(item.id);
          aborting = ex.cause;
        } else if (!Async.isShutdownException(ex)) {
          self._log.warn("Failed to reconcile incoming record " + item.id, ex);
          self._noteApplyFailure();
          failed.push(item.id);
          return;
        } else {
          throw ex;
        }
      }

      if (shouldApply) {
        count.applied++;
        applyBatch.push(item);
      } else {
        count.reconciled++;
        self._log.trace("Skipping reconciled incoming item " + item.id);
      }

      if (applyBatch.length == self.applyIncomingBatchSize) {
        doApplyBatch.call(self);
      }
      self._store._sleep(0);
    };

    // Only bother getting data from the server if there's new things
    if (this.lastModified == null || this.lastModified > this.lastSync) {
      let resp = newitems.getBatched();
      doApplyBatchAndPersistFailed.call(this);
      if (!resp.success) {
        resp.failureCode = ENGINE_DOWNLOAD_FAIL;
        throw resp;
      }

      if (aborting) {
        throw aborting;
      }
    }

    // Mobile: check if we got the maximum that we requested; get the rest if so.
    if (handled.length == newitems.limit) {
      let guidColl = new Collection(this.engineURL, null, this.service);

      // Sort and limit so that on mobile we only get the last X records.
      guidColl.limit = this.downloadLimit;
      guidColl.newer = this.lastSync;

      // index: Orders by the sortindex descending (highest weight first).
      guidColl.sort  = "index";

      let guids = guidColl.get();
      if (!guids.success)
        throw guids;

      // Figure out which guids weren't just fetched then remove any guids that
      // were already waiting and prepend the new ones
      let extra = Utils.arraySub(guids.obj, handled);
      if (extra.length > 0) {
        fetchBatch = Utils.arrayUnion(extra, fetchBatch);
        this.toFetch = Utils.arrayUnion(extra, this.toFetch);
      }
    }

    // Fast-foward the lastSync timestamp since we have stored the
    // remaining items in toFetch.
    if (this.lastSync < this.lastModified) {
      this.lastSync = this.lastModified;
    }

    // Process any backlog of GUIDs.
    // At this point we impose an upper limit on the number of items to fetch
    // in a single request, even for desktop, to avoid hitting URI limits.
    batchSize = isMobile ? this.mobileGUIDFetchBatchSize :
                           this.guidFetchBatchSize;

    while (fetchBatch.length && !aborting) {
      // Reuse the original query, but get rid of the restricting params
      // and batch remaining records.
      newitems.limit = 0;
      newitems.newer = 0;
      newitems.ids = fetchBatch.slice(0, batchSize);

      // Reuse the existing record handler set earlier
      let resp = newitems.get();
      if (!resp.success) {
        resp.failureCode = ENGINE_DOWNLOAD_FAIL;
        throw resp;
      }

      // This batch was successfully applied. Not using
      // doApplyBatchAndPersistFailed() here to avoid writing toFetch twice.
      fetchBatch = fetchBatch.slice(batchSize);
      this.toFetch = Utils.arraySub(this.toFetch, newitems.ids);
      this.previousFailed = Utils.arrayUnion(this.previousFailed, failed);
      if (failed.length) {
        count.failed += failed.length;
        this._log.debug("Records that failed to apply: " + failed);
      }
      failed = [];

      if (aborting) {
        throw aborting;
      }

      if (this.lastSync < this.lastModified) {
        this.lastSync = this.lastModified;
      }
    }

    // Apply remaining items.
    doApplyBatchAndPersistFailed.call(this);

    count.newFailed = this.previousFailed.reduce((count, engine) => {
      if (failedInPreviousSync.indexOf(engine) == -1) {
        count++;
        this._noteApplyNewFailure();
      }
      return count;
    }, 0);
    count.succeeded = Math.max(0, count.applied - count.failed);
    this._log.info(["Records:",
                    count.applied, "applied,",
                    count.succeeded, "successfully,",
                    count.failed, "failed to apply,",
                    count.newFailed, "newly failed to apply,",
                    count.reconciled, "reconciled."].join(" "));
    Observers.notify("weave:engine:sync:applied", count, this.name);
  },

  // Indicates whether an incoming item should be deleted from the server at
  // the end of the sync. Engines can override this method to clean up records
  // that shouldn't be on the server.
  _shouldDeleteRemotely(remoteItem) {
    return false;
  },

  _noteApplyFailure: function () {
    // here would be a good place to record telemetry...
  },

  _noteApplyNewFailure: function () {
    // here would be a good place to record telemetry...
  },

  /**
   * Find a GUID of an item that is a duplicate of the incoming item but happens
   * to have a different GUID
   *
   * @return GUID of the similar item; falsy otherwise
   */
  _findDupe: function (item) {
    // By default, assume there's no dupe items for the engine
  },

  // Called when the server has a record marked as deleted, but locally we've
  // changed it more recently than the deletion. If we return false, the
  // record will be deleted locally. If we return true, we'll reupload the
  // record to the server -- any extra work that's needed as part of this
  // process should be done at this point (such as mark the record's parent
  // for reuploading in the case of bookmarks).
  _shouldReviveRemotelyDeletedRecord(remoteItem) {
    return true;
  },

  _deleteId: function (id) {
    this._tracker.removeChangedID(id);

    // Remember this id to delete at the end of sync
    if (this._delete.ids == null)
      this._delete.ids = [id];
    else
      this._delete.ids.push(id);
  },

  _switchItemToDupe(localDupeGUID, incomingItem) {
    // The local, duplicate ID is always deleted on the server.
    this._deleteId(localDupeGUID);

    // We unconditionally change the item's ID in case the engine knows of
    // an item but doesn't expose it through itemExists. If the API
    // contract were stronger, this could be changed.
    this._log.debug("Switching local ID to incoming: " + localDupeGUID + " -> " +
                    incomingItem.id);
    this._store.changeItemID(localDupeGUID, incomingItem.id);
  },

  /**
   * Reconcile incoming record with local state.
   *
   * This function essentially determines whether to apply an incoming record.
   *
   * @param  item
   *         Record from server to be tested for application.
   * @return boolean
   *         Truthy if incoming record should be applied. False if not.
   */
  _reconcile: function (item) {
    if (this._log.level <= Log.Level.Trace) {
      this._log.trace("Incoming: " + item);
    }

    // We start reconciling by collecting a bunch of state. We do this here
    // because some state may change during the course of this function and we
    // need to operate on the original values.
    let existsLocally   = this._store.itemExists(item.id);
    let locallyModified = this._modified.has(item.id);

    // TODO Handle clock drift better. Tracked in bug 721181.
    let remoteAge = AsyncResource.serverTime - item.modified;
    let localAge  = locallyModified ?
      (Date.now() / 1000 - this._modified.getModifiedTimestamp(item.id)) : null;
    let remoteIsNewer = remoteAge < localAge;

    this._log.trace("Reconciling " + item.id + ". exists=" +
                    existsLocally + "; modified=" + locallyModified +
                    "; local age=" + localAge + "; incoming age=" +
                    remoteAge);

    // We handle deletions first so subsequent logic doesn't have to check
    // deleted flags.
    if (item.deleted) {
      // If the item doesn't exist locally, there is nothing for us to do. We
      // can't check for duplicates because the incoming record has no data
      // which can be used for duplicate detection.
      if (!existsLocally) {
        this._log.trace("Ignoring incoming item because it was deleted and " +
                        "the item does not exist locally.");
        return false;
      }

      // We decide whether to process the deletion by comparing the record
      // ages. If the item is not modified locally, the remote side wins and
      // the deletion is processed. If it is modified locally, we take the
      // newer record.
      if (!locallyModified) {
        this._log.trace("Applying incoming delete because the local item " +
                        "exists and isn't modified.");
        return true;
      }
      this._log.trace("Incoming record is deleted but we had local changes.");

      if (remoteIsNewer) {
        this._log.trace("Remote record is newer -- deleting local record.");
        return true;
      }
      // If the local record is newer, we defer to individual engines for
      // how to handle this. By default, we revive the record.
      let willRevive = this._shouldReviveRemotelyDeletedRecord(item);
      this._log.trace("Local record is newer -- reviving? " + willRevive);

      return !willRevive;
    }

    // At this point the incoming record is not for a deletion and must have
    // data. If the incoming record does not exist locally, we check for a local
    // duplicate existing under a different ID. The default implementation of
    // _findDupe() is empty, so engines have to opt in to this functionality.
    //
    // If we find a duplicate, we change the local ID to the incoming ID and we
    // refresh the metadata collected above. See bug 710448 for the history
    // of this logic.
    if (!existsLocally) {
      let localDupeGUID = this._findDupe(item);
      if (localDupeGUID) {
        this._log.trace("Local item " + localDupeGUID + " is a duplicate for " +
                        "incoming item " + item.id);

        // The current API contract does not mandate that the ID returned by
        // _findDupe() actually exists. Therefore, we have to perform this
        // check.
        existsLocally = this._store.itemExists(localDupeGUID);

        // If the local item was modified, we carry its metadata forward so
        // appropriate reconciling can be performed.
        if (this._modified.has(localDupeGUID)) {
          locallyModified = true;
          localAge = this._tracker._now() - this._modified.getModifiedTimestamp(localDupeGUID);
          remoteIsNewer = remoteAge < localAge;

          this._modified.swap(localDupeGUID, item.id);
        } else {
          locallyModified = false;
          localAge = null;
        }

        // Tell the engine to do whatever it needs to switch the items.
        this._switchItemToDupe(localDupeGUID, item);

        this._log.debug("Local item after duplication: age=" + localAge +
                        "; modified=" + locallyModified + "; exists=" +
                        existsLocally);
      } else {
        this._log.trace("No duplicate found for incoming item: " + item.id);
      }
    }

    // At this point we've performed duplicate detection. But, nothing here
    // should depend on duplicate detection as the above should have updated
    // state seamlessly.

    if (!existsLocally) {
      // If the item doesn't exist locally and we have no local modifications
      // to the item (implying that it was not deleted), always apply the remote
      // item.
      if (!locallyModified) {
        this._log.trace("Applying incoming because local item does not exist " +
                        "and was not deleted.");
        return true;
      }

      // If the item was modified locally but isn't present, it must have
      // been deleted. If the incoming record is younger, we restore from
      // that record.
      if (remoteIsNewer) {
        this._log.trace("Applying incoming because local item was deleted " +
                        "before the incoming item was changed.");
        this._modified.delete(item.id);
        return true;
      }

      this._log.trace("Ignoring incoming item because the local item's " +
                      "deletion is newer.");
      return false;
    }

    // If the remote and local records are the same, there is nothing to be
    // done, so we don't do anything. In the ideal world, this logic wouldn't
    // be here and the engine would take a record and apply it. The reason we
    // want to defer this logic is because it would avoid a redundant and
    // possibly expensive dip into the storage layer to query item state.
    // This should get addressed in the async rewrite, so we ignore it for now.
    let localRecord = this._createRecord(item.id);
    let recordsEqual = Utils.deepEquals(item.cleartext,
                                        localRecord.cleartext);

    // If the records are the same, we don't need to do anything. This does
    // potentially throw away a local modification time. But, if the records
    // are the same, does it matter?
    if (recordsEqual) {
      this._log.trace("Ignoring incoming item because the local item is " +
                      "identical.");

      this._modified.delete(item.id);
      return false;
    }

    // At this point the records are different.

    // If we have no local modifications, always take the server record.
    if (!locallyModified) {
      this._log.trace("Applying incoming record because no local conflicts.");
      return true;
    }

    // At this point, records are different and the local record is modified.
    // We resolve conflicts by record age, where the newest one wins. This does
    // result in data loss and should be handled by giving the engine an
    // opportunity to merge the records. Bug 720592 tracks this feature.
    this._log.warn("DATA LOSS: Both local and remote changes to record: " +
                   item.id);
    return remoteIsNewer;
  },

  // Upload outgoing records.
  _uploadOutgoing: function () {
    this._log.trace("Uploading local changes to server.");

    let modifiedIDs = this._modified.ids();
    if (modifiedIDs.length) {
      this._log.trace("Preparing " + modifiedIDs.length +
                      " outgoing records");

      let counts = { sent: modifiedIDs.length, failed: 0 };

      // collection we'll upload
      let up = new Collection(this.engineURL, null, this.service);

      let failed = [];
      let successful = [];
      let handleResponse = (resp, batchOngoing = false) => {
        // Note: We don't want to update this.lastSync, or this._modified until
        // the batch is complete, however we want to remember success/failure
        // indicators for when that happens.
        if (!resp.success) {
          this._log.debug("Uploading records failed: " + resp);
          resp.failureCode = resp.status == 412 ? ENGINE_BATCH_INTERRUPTED : ENGINE_UPLOAD_FAIL;
          throw resp;
        }

        // Update server timestamp from the upload.
        failed = failed.concat(Object.keys(resp.obj.failed));
        successful = successful.concat(resp.obj.success);

        if (batchOngoing) {
          // Nothing to do yet
          return;
        }
        // Advance lastSync since we've finished the batch.
        let modified = resp.headers["x-weave-timestamp"];
        if (modified > this.lastSync) {
          this.lastSync = modified;
        }
        if (failed.length && this._log.level <= Log.Level.Debug) {
          this._log.debug("Records that will be uploaded again because "
                          + "the server couldn't store them: "
                          + failed.join(", "));
        }

        counts.failed += failed.length;

        for (let id of successful) {
          this._modified.delete(id);
        }

        this._onRecordsWritten(successful, failed);

        // clear for next batch
        failed.length = 0;
        successful.length = 0;
      };

      let postQueue = up.newPostQueue(this._log, this.lastSync, handleResponse);

      for (let id of modifiedIDs) {
        let out;
        let ok = false;
        try {
          out = this._createRecord(id);
          if (this._log.level <= Log.Level.Trace)
            this._log.trace("Outgoing: " + out);

          out.encrypt(this.service.collectionKeys.keyForCollection(this.name));
          ok = true;
        } catch (ex) {
          if (Async.isShutdownException(ex)) {
            throw ex;
          }
          this._log.warn("Error creating record", ex);
        }
        if (ok) {
          let { enqueued, error } = postQueue.enqueue(out);
          if (!enqueued) {
            ++counts.failed;
            if (!this.allowSkippedRecord) {
              throw error;
            }
          }
        }
        this._store._sleep(0);
      }
      postQueue.flush(true);
      Observers.notify("weave:engine:sync:uploaded", counts, this.name);
    }
  },

  _onRecordsWritten(succeeded, failed) {
    // Implement this method to take specific actions against successfully
    // uploaded records and failed records.
  },

  // Any cleanup necessary.
  // Save the current snapshot so as to calculate changes at next sync
  _syncFinish: function () {
    this._log.trace("Finishing up sync");
    this._tracker.resetScore();

    let doDelete = Utils.bind2(this, function(key, val) {
      let coll = new Collection(this.engineURL, this._recordObj, this.service);
      coll[key] = val;
      coll.delete();
    });

    for (let [key, val] of Object.entries(this._delete)) {
      // Remove the key for future uses
      delete this._delete[key];

      // Send a simple delete for the property
      if (key != "ids" || val.length <= 100)
        doDelete(key, val);
      else {
        // For many ids, split into chunks of at most 100
        while (val.length > 0) {
          doDelete(key, val.slice(0, 100));
          val = val.slice(100);
        }
      }
    }
  },

  _syncCleanup: function () {
    if (!this._modified) {
      return;
    }

    // Mark failed WBOs as changed again so they are reuploaded next time.
    this.trackRemainingChanges();
    this._modified.clear();
  },

  _sync: function () {
    try {
      this._syncStartup();
      Observers.notify("weave:engine:sync:status", "process-incoming");
      this._processIncoming();
      Observers.notify("weave:engine:sync:status", "upload-outgoing");
      this._uploadOutgoing();
      this._syncFinish();
    } finally {
      this._syncCleanup();
    }
  },

  canDecrypt: function () {
    // Report failure even if there's nothing to decrypt
    let canDecrypt = false;

    // Fetch the most recently uploaded record and try to decrypt it
    let test = new Collection(this.engineURL, this._recordObj, this.service);
    test.limit = 1;
    test.sort = "newest";
    test.full = true;

    let key = this.service.collectionKeys.keyForCollection(this.name);
    test.recordHandler = function recordHandler(record) {
      record.decrypt(key);
      canDecrypt = true;
    }.bind(this);

    // Any failure fetching/decrypting will just result in false
    try {
      this._log.trace("Trying to decrypt a record from the server..");
      test.get();
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      this._log.debug("Failed test decrypt", ex);
    }

    return canDecrypt;
  },

  _resetClient: function () {
    this.resetLastSync();
    this.previousFailed = [];
    this.toFetch = [];
  },

  wipeServer: function () {
    let response = this.service.resource(this.engineURL).delete();
    if (response.status != 200 && response.status != 404) {
      throw response;
    }
    this._resetClient();
  },

  removeClientData: function () {
    // Implement this method in engines that store client specific data
    // on the server.
  },

  /*
   * Decide on (and partially effect) an error-handling strategy.
   *
   * Asks the Service to respond to an HMAC error, which might result in keys
   * being downloaded. That call returns true if an action which might allow a
   * retry to occur.
   *
   * If `mayRetry` is truthy, and the Service suggests a retry,
   * handleHMACMismatch returns kRecoveryStrategy.retry. Otherwise, it returns
   * kRecoveryStrategy.error.
   *
   * Subclasses of SyncEngine can override this method to allow for different
   * behavior -- e.g., to delete and ignore erroneous entries.
   *
   * All return values will be part of the kRecoveryStrategy enumeration.
   */
  handleHMACMismatch: function (item, mayRetry) {
    // By default we either try again, or bail out noisily.
    return (this.service.handleHMACEvent() && mayRetry) ?
           SyncEngine.kRecoveryStrategy.retry :
           SyncEngine.kRecoveryStrategy.error;
  },

  /**
   * Returns a changeset containing all items in the store. The default
   * implementation returns a changeset with timestamps from long ago, to
   * ensure we always use the remote version if one exists.
   *
   * This function is only called for the first sync. Subsequent syncs call
   * `pullNewChanges`.
   *
   * @return A `Changeset` object.
   */
  pullAllChanges() {
    let changeset = new Changeset();
    for (let id in this._store.getAllIDs()) {
      changeset.set(id, 0);
    }
    return changeset;
  },

  /*
   * Returns a changeset containing entries for all currently tracked items.
   * The default implementation returns a changeset with timestamps indicating
   * when the item was added to the tracker.
   *
   * @return A `Changeset` object.
   */
  pullNewChanges() {
    return new Changeset(this.getChangedIDs());
  },

  /**
   * Adds all remaining changeset entries back to the tracker, typically for
   * items that failed to upload. This method is called at the end of each sync.
   *
   */
  trackRemainingChanges() {
    for (let [id, change] of this._modified.entries()) {
      this._tracker.addChangedID(id, change);
    }
  },
};

/**
 * A changeset is created for each sync in `Engine::get{Changed, All}IDs`,
 * and stores opaque change data for tracked IDs. The default implementation
 * only records timestamps, though engines can extend this to store additional
 * data for each entry.
 */
class Changeset {
  // Creates a changeset with an initial set of tracked entries.
  constructor(changes = {}) {
    this.changes = changes;
  }

  // Returns the last modified time, in seconds, for an entry in the changeset.
  // `id` is guaranteed to be in the set.
  getModifiedTimestamp(id) {
    return this.changes[id];
  }

  // Adds a change for a tracked ID to the changeset.
  set(id, change) {
    this.changes[id] = change;
  }

  // Indicates whether an entry is in the changeset.
  has(id) {
    return id in this.changes;
  }

  // Deletes an entry from the changeset. Used to clean up entries for
  // reconciled and successfully uploaded records.
  delete(id) {
    delete this.changes[id];
  }

  // Swaps two entries in the changeset. Used when reconciling duplicates that
  // have local changes.
  swap(oldID, newID) {
    this.changes[newID] = this.changes[oldID];
    delete this.changes[oldID];
  }

  // Returns an array of all tracked IDs in this changeset.
  ids() {
    return Object.keys(this.changes);
  }

  // Returns an array of `[id, change]` tuples. Used to repopulate the tracker
  // with entries for failed uploads at the end of a sync.
  entries() {
    return Object.entries(this.changes);
  }

  // Returns the number of entries in this changeset.
  count() {
    return this.ids().length;
  }

  // Clears the changeset.
  clear() {
    this.changes = {};
  }
}
