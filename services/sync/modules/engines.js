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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/JSONFile.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

XPCOMUtils.defineLazyModuleGetters(this, {
  fxAccounts: "resource://gre/modules/FxAccounts.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesSyncUtils: "resource://gre/modules/PlacesSyncUtils.jsm",
});

function ensureDirectory(path) {
  let basename = OS.Path.dirname(path);
  return OS.File.makeDir(basename, { from: OS.Constants.Path.profileDir });
}

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

  this._log = Log.repository.getLogger(`Sync.Engine.${name}.Tracker`);

  this._score = 0;
  this._ignored = [];
  this._storage = new JSONFile({
    path: Utils.jsonFilePath("changes/" + this.file),
    dataPostProcessor: json => this._dataPostProcessor(json),
    beforeSave: () => this._beforeSave(),
  });
  this.ignoreAll = false;

  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);

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

  // Default to an empty object if the file doesn't exist.
  _dataPostProcessor(json) {
    return typeof json == "object" && json || {};
  },

  // Ensure the Weave storage directory exists before writing the file.
  _beforeSave() {
    return ensureDirectory(this._storage.path);
  },

  get changedIDs() {
    Async.promiseSpinningly(this._storage.load());
    return this._storage.data;
  },

  set score(value) {
    this._score = value;
    Observers.notify("weave:engine:score:updated", this.name);
  },

  // Should be called by service everytime a sync has been done for an engine
  resetScore() {
    this._score = 0;
  },

  persistChangedIDs: true,

  _saveChangedIDs() {
    if (!this.persistChangedIDs) {
      this._log.debug("Not saving changedIDs.");
      return;
    }
    this._storage.saveSoon();
  },

  // ignore/unignore specific IDs.  Useful for ignoring items that are
  // being processed, or that shouldn't be synced.
  // But note: not persisted to disk

  ignoreID(id) {
    this.unignoreID(id);
    this._ignored.push(id);
  },

  unignoreID(id) {
    let index = this._ignored.indexOf(id);
    if (index != -1)
      this._ignored.splice(index, 1);
  },

  _saveChangedID(id, when) {
    this._log.trace(`Adding changed ID: ${id}, ${JSON.stringify(when)}`);
    this.changedIDs[id] = when;
    this._saveChangedIDs();
  },

  addChangedID(id, when) {
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

  removeChangedID(...ids) {
    if (!ids.length || this.ignoreAll) {
      return false;
    }
    for (let id of ids) {
      if (!id) {
        this._log.warn("Attempted to remove undefined ID from tracker");
        continue;
      }
      if (this._ignored.includes(id)) {
        this._log.debug(`Not removing ignored ID ${id} from tracker`);
        continue;
      }
      if (this.changedIDs[id] != null) {
        this._log.trace("Removing changed ID " + id);
        delete this.changedIDs[id];
      }
    }
    this._saveChangedIDs();
    return true;
  },

  clearChangedIDs() {
    this._log.trace("Clearing changed ID list");
    this._storage.data = {};
    this._saveChangedIDs();
  },

  _now() {
    return Date.now() / 1000;
  },

  _isTracking: false,

  // Override these in your subclasses.
  startTracking() {
  },

  stopTracking() {
  },

  engineIsEnabled() {
    if (!this.engine) {
      // Can't tell -- we must be running in a test!
      return true;
    }
    return this.engine.enabled;
  },

  onEngineEnabledChanged(engineEnabled) {
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

  observe(subject, topic, data) {
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
    }
  },

  async finalize() {
    // Stop listening for tracking and engine enabled change notifications.
    // Important for tests where we unregister the engine during cleanup.
    Svc.Obs.remove("weave:engine:start-tracking", this);
    Svc.Obs.remove("weave:engine:stop-tracking", this);

    // Persist all pending tracked changes to disk, and wait for the final write
    // to finish.
    this._saveChangedIDs();
    await this._storage.finalize();
  },
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

  this._log = Log.repository.getLogger(`Sync.Engine.${name}.Store`);

  XPCOMUtils.defineLazyGetter(this, "_timer", function() {
    return Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  });
};
Store.prototype = {

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
  async applyIncomingBatch(records) {
    let failed = [];
    let maybeYield = Async.jankYielder();
    for (let record of records) {
      await maybeYield();
      try {
        await this.applyIncoming(record);
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
        failed.push(record.id);
      }
    }
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
  async applyIncoming(record) {
    if (record.deleted)
      await this.remove(record);
    else if (!(await this.itemExists(record.id)))
      await this.create(record);
    else
      await this.update(record);
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
  async create(record) {
    throw new Error("override create in a subclass");
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
  async remove(record) {
    throw new Error("override remove in a subclass");
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
  async update(record) {
    throw new Error("override update in a subclass");
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
  async itemExists(id) {
    throw new Error("override itemExists in a subclass");
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
  async createRecord(id, collection) {
    throw new Error("override createRecord in a subclass");
  },

  /**
   * Change the ID of a record.
   *
   * @param  oldID
   *         string old/current record ID
   * @param  newID
   *         string new record ID
   */
  async changeItemID(oldID, newID) {
    throw new Error("override changeItemID in a subclass");
  },

  /**
   * Obtain the set of all known record IDs.
   *
   * @return Object with ID strings as keys and values of true. The values
   *         are ignored.
   */
  async getAllIDs() {
    throw new Error("override getAllIDs in a subclass");
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
  async wipe() {
    throw new Error("override wipe in a subclass");
  }
};

this.EngineManager = function EngineManager(service) {
  this.service = service;

  this._engines = {};

  // This will be populated by Service on startup.
  this._declined = new Set();
  this._log = Log.repository.getLogger("Sync.EngineManager");
  this._log.manageLevelFromPref("services.sync.log.logger.service.engines");
  // define the default level for all engine logs here (although each engine
  // allows its level to be controlled via a specific, non-default pref)
  Log.repository.getLogger(`Sync.Engine`).manageLevelFromPref("services.sync.log.logger.engine");
};
EngineManager.prototype = {
  get(name) {
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

  getAll() {
    let engines = [];
    for (let [, engine] of Object.entries(this._engines)) {
      engines.push(engine);
    }
    return engines;
  },

  /**
   * N.B., does not pay attention to the declined list.
   */
  getEnabled() {
    return this.getAll()
               .filter((engine) => engine.enabled)
               .sort((a, b) => a.syncPriority - b.syncPriority);
  },

  get enabledEngineNames() {
    return this.getEnabled().map(e => e.name);
  },

  persistDeclined() {
    Svc.Prefs.set("declinedEngines", [...this._declined].join(","));
  },

  /**
   * Returns an array.
   */
  getDeclined() {
    return [...this._declined];
  },

  setDeclined(engines) {
    this._declined = new Set(engines);
    this.persistDeclined();
  },

  isDeclined(engineName) {
    return this._declined.has(engineName);
  },

  /**
   * Accepts a Set or an array.
   */
  decline(engines) {
    for (let e of engines) {
      this._declined.add(e);
    }
    this.persistDeclined();
  },

  undecline(engines) {
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
  declineDisabled() {
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
  async register(engineObject) {
    if (Array.isArray(engineObject)) {
      for (const e of engineObject) {
        await this.register(e);
      }
      return;
    }

    try {
      let engine = new engineObject(this.service);
      let name = engine.name;
      if (name in this._engines) {
        this._log.error("Engine '" + name + "' is already registered!");
      } else {
        if (engine.initialize) {
          await engine.initialize();
        }
        this._engines[name] = engine;
      }
    } catch (ex) {
      let name = engineObject || "";
      name = name.prototype || "";
      name = name.name || "";

      this._log.error(`Could not initialize engine ${name}`, ex);
    }
  },

  unregister(val) {
    let name = val;
    if (val instanceof Engine) {
      name = val.name;
    }
    if (name in this._engines) {
      let engine = this._engines[name];
      delete this._engines[name];
      Async.promiseSpinningly(engine.finalize());
    }
  },

  clear() {
    for (let name in this._engines) {
      let engine = this._engines[name];
      delete this._engines[name];
      Async.promiseSpinningly(engine.finalize());
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
  this._log.manageLevelFromPref(`services.sync.log.logger.engine.${this.name}`);

  this._modified = this.emptyChangeset();
  this._tracker; // initialize tracker to load previously changed IDs
  this._log.debug("Engine constructed");

  XPCOMUtils.defineLazyPreferenceGetter(this, "_enabled",
    `services.sync.engine.${this.prefName}`, false,
    (data, previous, latest) =>
      this._tracker.onEngineEnabledChanged(latest));
};
Engine.prototype = {
  // _storeObj, and _trackerObj should to be overridden in subclasses
  _storeObj: Store,
  _trackerObj: Tracker,

  // Override this method to return a new changeset type.
  emptyChangeset() {
    return new Changeset();
  },

  // Local 'constant'.
  // Signal to the engine that processing further records is pointless.
  eEngineAbortApplyIncoming: "error.engine.abort.applyincoming",

  // Should we keep syncing if we find a record that cannot be uploaded (ever)?
  // If this is false, we'll throw, otherwise, we'll ignore the record and
  // continue. This currently can only happen due to the record being larger
  // than the record upload limit.
  allowSkippedRecord: true,

  get prefName() {
    return this.name;
  },

  get enabled() {
    return this._enabled;
  },

  set enabled(val) {
    if (!!val != this._enabled) {
      Svc.Prefs.set("engine." + this.prefName, !!val);
    }
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

  async sync() {
    if (!this.enabled) {
      return false;
    }

    if (!this._sync) {
      throw new Error("engine does not implement _sync method");
    }

    return this._notify("sync", this.name, this._sync)();
  },

  /**
   * Get rid of any local meta-data.
   */
  async resetClient() {
    if (!this._resetClient) {
      throw new Error("engine does not implement _resetClient method");
    }

    return this._notify("reset-client", this.name, this._resetClient)();
  },

  async _wipeClient() {
    await this.resetClient();
    this._log.debug("Deleting all local data");
    this._tracker.ignoreAll = true;
    await this._store.wipe();
    this._tracker.ignoreAll = false;
    this._tracker.clearChangedIDs();
  },

  async wipeClient() {
    return this._notify("wipe-client", this.name, this._wipeClient)();
  },

  /**
   * If one exists, initialize and return a validator for this engine (which
   * must have a `validate(engine)` method that returns a promise to an object
   * with a getSummary method). Otherwise return null.
   */
  getValidator() {
    return null;
  },

  async finalize() {
    await this._tracker.finalize();
  },
};

this.SyncEngine = function SyncEngine(name, service) {
  Engine.call(this, name || "SyncEngine", service);

  this._toFetchStorage = new JSONFile({
    path: Utils.jsonFilePath("toFetch/" + this.name),
    dataPostProcessor: json => this._metadataPostProcessor(json),
    beforeSave: () => this._beforeSaveMetadata(),
  });

  this._previousFailedStorage = new JSONFile({
    path: Utils.jsonFilePath("failed/" + this.name),
    dataPostProcessor: json => this._metadataPostProcessor(json),
    beforeSave: () => this._beforeSaveMetadata(),
  });
  Utils.defineLazyIDProperty(this, "syncID", `services.sync.${this.name}.syncID`);

  XPCOMUtils.defineLazyPreferenceGetter(this, "_lastSync",
                                        `services.sync.${this.name}.lastSync`,
                                        "0", null,
                                        v => parseFloat(v));
  XPCOMUtils.defineLazyPreferenceGetter(this, "_lastSyncLocal",
                                        `services.sync.${this.name}.lastSyncLocal`,
                                        "0", null,
                                        v => parseInt(v, 10));
  // Async initializations can be made in the initialize() method.

  // The map of ids => metadata for records needing a weak upload.
  //
  // Currently the "metadata" fields are:
  //
  // - forceTombstone: whether or not we should ignore the local information
  //   about the record, and write a tombstone for it anyway -- e.g. in the case
  //   of records that should exist locally, but should never be uploaded to the
  //   server (note that not all sync engines support tombstones)
  //
  // The difference between this and a "normal" upload is that these records
  // are only tracked in memory, and if the upload attempt fails (shutdown,
  // 412, etc), we abort uploading the "weak" set (by clearing the map).
  //
  // The rationale here is for the cases where we receive a record from the
  // server that we know is wrong in some (small) way. For example, the
  // dateAdded field on bookmarks -- maybe we have a better date, or the server
  // record is entirely missing the date, etc.
  //
  // In these cases, we fix our local copy of the record, and mark it for
  // weak upload. A normal ("strong") upload is problematic here because
  // in the case of a conflict from the server, there's a window where our
  // record would be marked as modified more recently than a change that occurs
  // on another device change, and we lose data from the user.
  //
  // Additionally, we use this as the set of items to upload for bookmark
  // repair reponse, which has similar constraints.
  this._needWeakUpload = new Map();
};

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

  _metadataPostProcessor(json) {
    if (Array.isArray(json)) {
      // Pre-`JSONFile` storage stored an array, but `JSONFile` defaults to
      // an object, so we wrap the array for consistency.
      return { ids: json };
    }
    if (!json.ids) {
      json.ids = [];
    }
    return json;
  },

  async _beforeSaveMetadata() {
    await ensureDirectory(this._toFetchStorage.path);
    await ensureDirectory(this._previousFailedStorage.path);
  },

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

  downloadBatchSize: DEFAULT_DOWNLOAD_BATCH_SIZE,

  async initialize() {
    await this._toFetchStorage.load();
    await this._previousFailedStorage.load();
    this._log.debug("SyncEngine initialized", this.name);
  },

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

  /*
   * lastSync is a timestamp in server time.
   */
  get lastSync() {
    return this._lastSync;
  },
  set lastSync(value) {
    // Store the value as a string to keep floating point precision
    Svc.Prefs.set(this.name + ".lastSync", value.toString());
  },
  resetLastSync() {
    this._log.debug("Resetting " + this.name + " last sync time");
    Svc.Prefs.set(this.name + ".lastSync", "0");
    this.lastSyncLocal = 0;
  },

  get toFetch() {
    this._toFetchStorage.ensureDataReady();
    return this._toFetchStorage.data.ids;
  },

  set toFetch(ids) {
    this._toFetchStorage.data = { ids };
    this._toFetchStorage.saveSoon();
  },

  get previousFailed() {
    this._previousFailedStorage.ensureDataReady();
    return this._previousFailedStorage.data.ids;
  },
  set previousFailed(ids) {
    this._previousFailedStorage.data = { ids };
    this._previousFailedStorage.saveSoon();
  },

  /*
   * lastSyncLocal is a timestamp in local time.
   */
  get lastSyncLocal() {
    return this._lastSyncLocal;
  },
  set lastSyncLocal(value) {
    // Store as a string because pref can only store C longs as numbers.
    Svc.Prefs.set(this.name + ".lastSyncLocal", value.toString());
  },

  /*
   * Returns a changeset for this sync. Engine implementations can override this
   * method to bypass the tracker for certain or all changed items.
   */
  async getChangedIDs() {
    return this._tracker.changedIDs;
  },

  // Create a new record using the store and add in metadata.
  async _createRecord(id) {
    let record = await this._store.createRecord(id, this.name);
    record.id = id;
    record.collection = this.name;
    return record;
  },

  // Creates a tombstone Sync record with additional metadata.
  _createTombstone(id) {
    let tombstone = new this._recordObj(this.name, id);
    tombstone.id = id;
    tombstone.collection = this.name;
    tombstone.deleted = true;
    return tombstone;
  },

  addForWeakUpload(id, { forceTombstone = false } = {}) {
    this._needWeakUpload.set(id, { forceTombstone });
  },

  // Any setup that needs to happen at the beginning of each sync.
  async _syncStartup() {

    // Determine if we need to wipe on outdated versions
    let metaGlobal = await this.service.recordManager.get(this.metaURL);
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
    } else if (engineData.version > this.version) {
      // Don't sync this engine if the server has newer data

      // Changes below need to be processed in bug 1295510 that's why eslint is ignored
      // eslint-disable-next-line no-new-wrappers
      let error = new String("New data: " + [engineData.version, this.version]);
      error.failureCode = VERSION_OUT_OF_DATE;
      throw error;
    } else if (engineData.syncID != this.syncID) {
      // Changes to syncID mean we'll need to upload everything
      this._log.debug("Engine syncIDs: " + [engineData.syncID, this.syncID]);
      this.syncID = engineData.syncID;
      await this._resetClient();
    }

    // Delete any existing data and reupload on bad version or missing meta.
    // No crypto component here...? We could regenerate per-collection keys...
    if (needsWipe) {
      await this.wipeServer();
    }

    // Save objects that need to be uploaded in this._modified. We also save
    // the timestamp of this fetch in this.lastSyncLocal. As we successfully
    // upload objects we remove them from this._modified. If an error occurs
    // or any objects fail to upload, they will remain in this._modified. At
    // the end of a sync, or after an error, we add all objects remaining in
    // this._modified to the tracker.
    this.lastSyncLocal = Date.now();
    let initialChanges;
    if (this.lastSync) {
      initialChanges = await this.pullNewChanges();
    } else {
      this._log.debug("First sync, uploading all items");
      initialChanges = await this.pullAllChanges();
    }
    this._modified.replace(initialChanges);
    // Clear the tracker now. If the sync fails we'll add the ones we failed
    // to upload back.
    this._tracker.clearChangedIDs();
    this._tracker.resetScore();

    this._log.info(this._modified.count() +
                   " outgoing items pre-reconciliation");

    // Keep track of what to delete at the end of sync
    this._delete = {};
  },

  /**
   * A tiny abstraction to make it easier to test incoming record
   * application.
   */
  itemSource() {
    return new Collection(this.engineURL, this._recordObj, this.service);
  },

  /**
   * Download and apply remote records changed since the last sync. This
   * happens in three stages.
   *
   * In the first stage, we fetch full records for all changed items, newest
   * first, up to the download limit. The limit lets us make progress for large
   * collections, where the sync is likely to be interrupted before we
   * can fetch everything.
   *
   * In the second stage, we fetch the IDs of any remaining records changed
   * since the last sync, add them to our backlog, and fast-forward our last
   * sync time.
   *
   * In the third stage, we fetch and apply records for all backlogged IDs,
   * as well as any records that failed to apply during the last sync. We
   * request records for the IDs in chunks, to avoid exceeding URL length
   * limits, then remove successfully applied records from the backlog, and
   * record IDs of any records that failed to apply to retry on the next sync.
   */
  async _processIncoming() {
    this._log.trace("Downloading & applying server changes");

    let newitems = this.itemSource();

    newitems.newer = this.lastSync;
    newitems.full  = true;

    let downloadLimit = Infinity;
    if (this.downloadLimit) {
      // Fetch new records up to the download limit. Currently, only the history
      // engine sets a limit, since the history collection has the highest volume
      // of changed records between syncs. The other engines fetch all records
      // changed since the last sync.
      if (this._defaultSort) {
        // A download limit with a sort order doesn't make sense: we won't know
        // which records to backfill.
        throw new Error("Can't specify download limit with default sort order");
      }
      newitems.sort = "newest";
      downloadLimit = newitems.limit = this.downloadLimit;
    } else if (this._defaultSort) {
      // The bookmarks engine fetches records by sort index; other engines leave
      // the order unspecified. We can remove `_defaultSort` entirely after bug
      // 1305563: the sort index won't matter because we'll buffer all bookmarks
      // before applying.
      newitems.sort = this._defaultSort;
    }

    // applied    => number of items that should be applied.
    // failed     => number of items that failed in this sync.
    // newFailed  => number of items that failed for the first time in this sync.
    // reconciled => number of items that were reconciled.
    let count = {applied: 0, failed: 0, newFailed: 0, reconciled: 0};
    let recordsToApply = [];
    let failedInCurrentSync = [];

    let oldestModified = this.lastModified;
    let downloadedIDs = new Set();

    // Stage 1: Fetch new records from the server, up to the download limit.
    if (this.lastModified == null || this.lastModified > this.lastSync) {
      let { response, records } = await newitems.getBatched(this.downloadBatchSize);
      if (!response.success) {
        response.failureCode = ENGINE_DOWNLOAD_FAIL;
        throw response;
      }

      let maybeYield = Async.jankYielder();
      for (let record of records) {
        await maybeYield();
        downloadedIDs.add(record.id);

        if (record.modified < oldestModified) {
          oldestModified = record.modified;
        }

        let { shouldApply, error } = await this._maybeReconcile(record);
        if (error) {
          failedInCurrentSync.push(record.id);
          count.failed++;
          continue;
        }
        if (!shouldApply) {
          count.reconciled++;
          continue;
        }
        recordsToApply.push(record);
      }

      let failedToApply = await this._applyRecords(recordsToApply);
      failedInCurrentSync.push(...failedToApply);

      // `applied` is a bit of a misnomer: it counts records that *should* be
      // applied, so it also includes records that we tried to apply and failed.
      // `recordsToApply.length - failedToApply.length` is the number of records
      // that we *successfully* applied.
      count.failed += failedToApply.length;
      count.applied += recordsToApply.length;
    }

    // Stage 2: If we reached our download limit, we might still have records
    // on the server that changed since the last sync. Fetch the IDs for the
    // remaining records, and add them to the backlog. Note that this stage
    // only runs for engines that set a download limit.
    if (downloadedIDs.size == downloadLimit) {
      let guidColl = this.itemSource();

      guidColl.newer = this.lastSync;
      guidColl.older = oldestModified;
      guidColl.sort  = "oldest";

      let guids = await guidColl.get();
      if (!guids.success)
        throw guids;

      // Filtering out already downloaded IDs here isn't necessary. We only do
      // that in case the Sync server doesn't support `older` (bug 1316110).
      let remainingIDs = guids.obj.filter(id => !downloadedIDs.has(id));
      if (remainingIDs.length > 0) {
        this.toFetch = Utils.arrayUnion(this.toFetch, remainingIDs);
      }
    }

    // Fast-foward the lastSync timestamp since we have backlogged the
    // remaining items.
    if (this.lastSync < this.lastModified) {
      this.lastSync = this.lastModified;
    }

    // Stage 3: Backfill records from the backlog, and those that failed to
    // decrypt or apply during the last sync. We only backfill up to the
    // download limit, to prevent a large backlog for one engine from blocking
    // the others. We'll keep processing the backlog on subsequent engine syncs.
    let failedInPreviousSync = this.previousFailed;
    let idsToBackfill = Utils.arrayUnion(this.toFetch.slice(0, downloadLimit),
      failedInPreviousSync);

    // Note that we intentionally overwrite the previously failed list here.
    // Records that fail to decrypt or apply in two consecutive syncs are likely
    // corrupt; we remove them from the list because retrying and failing on
    // every subsequent sync just adds noise.
    this.previousFailed = failedInCurrentSync;

    let backfilledItems = this.itemSource();

    backfilledItems.sort = "newest";
    backfilledItems.full = true;

    // `getBatched` includes the list of IDs as a query parameter, so we need to fetch
    // records in chunks to avoid exceeding URI length limits.
    for (let ids of PlacesSyncUtils.chunkArray(idsToBackfill, this.guidFetchBatchSize)) {
      backfilledItems.ids = ids;

      let {response, records} = await backfilledItems.getBatched(this.downloadBatchSize);
      if (!response.success) {
        response.failureCode = ENGINE_DOWNLOAD_FAIL;
        throw response;
      }

      let maybeYield = Async.jankYielder();
      let backfilledRecordsToApply = [];
      let failedInBackfill = [];

      for (let record of records) {
        await maybeYield();

        let { shouldApply, error } = await this._maybeReconcile(record);
        if (error) {
          failedInBackfill.push(record.id);
          count.failed++;
          continue;
        }
        if (!shouldApply) {
          count.reconciled++;
          continue;
        }
        backfilledRecordsToApply.push(record);
      }
      let failedToApply = await this._applyRecords(backfilledRecordsToApply);
      failedInBackfill.push(...failedToApply);

      count.failed += failedToApply.length;
      count.applied += backfilledRecordsToApply.length;

      this.toFetch = Utils.arraySub(this.toFetch, ids);
      this.previousFailed = Utils.arrayUnion(this.previousFailed, failedInBackfill);

      if (this.lastSync < this.lastModified) {
        this.lastSync = this.lastModified;
      }
    }

    count.newFailed = this.previousFailed.reduce((count, engine) => {
      if (failedInPreviousSync.indexOf(engine) == -1) {
        count++;
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

  async _maybeReconcile(item) {
    let key = this.service.collectionKeys.keyForCollection(this.name);

    // Grab a later last modified if possible
    if (this.lastModified == null || item.modified > this.lastModified) {
      this.lastModified = item.modified;
    }

    try {
      try {
        await item.decrypt(key);
      } catch (ex) {
        if (!Utils.isHMACMismatch(ex)) {
          throw ex;
        }
        let strategy = await this.handleHMACMismatch(item, true);
        if (strategy == SyncEngine.kRecoveryStrategy.retry) {
          // You only get one retry.
          try {
            // Try decrypting again, typically because we've got new keys.
            this._log.info("Trying decrypt again...");
            key = this.service.collectionKeys.keyForCollection(this.name);
            await item.decrypt(key);
            strategy = null;
          } catch (ex) {
            if (!Utils.isHMACMismatch(ex)) {
              throw ex;
            }
            strategy = await this.handleHMACMismatch(item, false);
          }
        }

        switch (strategy) {
          case null:
            // Retry succeeded! No further handling.
            break;
          case SyncEngine.kRecoveryStrategy.retry:
            this._log.debug("Ignoring second retry suggestion.");
            // Fall through to error case.
          case SyncEngine.kRecoveryStrategy.error:
            this._log.warn("Error decrypting record", ex);
            return { shouldApply: false, error: ex };
          case SyncEngine.kRecoveryStrategy.ignore:
            this._log.debug("Ignoring record " + item.id +
                            " with bad HMAC: already handled.");
            return { shouldApply: false, error: null };
        }
      }
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      this._log.warn("Error decrypting record", ex);
      return { shouldApply: false, error: ex };
    }

    if (this._shouldDeleteRemotely(item)) {
      this._log.trace("Deleting item from server without applying", item);
      this._deleteId(item.id);
      return { shouldApply: false, error: null };
    }

    let shouldApply;
    try {
      shouldApply = await this._reconcile(item);
    } catch (ex) {
      if (ex.code == Engine.prototype.eEngineAbortApplyIncoming) {
        this._log.warn("Reconciliation failed: aborting incoming processing.");
        throw ex.cause;
      } else if (!Async.isShutdownException(ex)) {
        this._log.warn("Failed to reconcile incoming record " + item.id, ex);
        return { shouldApply: false, error: ex };
      } else {
        throw ex;
      }
    }

    if (!shouldApply) {
      this._log.trace("Skipping reconciled incoming item " + item.id);
    }

    return { shouldApply, error: null };
  },

  async _applyRecords(records) {
    this._tracker.ignoreAll = true;
    try {
      let failedIDs = await this._store.applyIncomingBatch(records);
      return failedIDs;
    } catch (ex) {
      // Catch any error that escapes from applyIncomingBatch. At present
      // those will all be abort events.
      this._log.warn("Got exception, aborting processIncoming", ex);
      throw ex;
    } finally {
      this._tracker.ignoreAll = false;
    }
  },

  // Indicates whether an incoming item should be deleted from the server at
  // the end of the sync. Engines can override this method to clean up records
  // that shouldn't be on the server.
  _shouldDeleteRemotely(remoteItem) {
    return false;
  },

  /**
   * Find a GUID of an item that is a duplicate of the incoming item but happens
   * to have a different GUID
   *
   * @return GUID of the similar item; falsy otherwise
   */
  async _findDupe(item) {
    // By default, assume there's no dupe items for the engine
  },

  /**
   * Called before a remote record is discarded due to failed reconciliation.
   * Used by bookmark sync to merge folder child orders.
   */
  beforeRecordDiscard(localRecord, remoteRecord, remoteIsNewer) {
  },

  // Called when the server has a record marked as deleted, but locally we've
  // changed it more recently than the deletion. If we return false, the
  // record will be deleted locally. If we return true, we'll reupload the
  // record to the server -- any extra work that's needed as part of this
  // process should be done at this point (such as mark the record's parent
  // for reuploading in the case of bookmarks).
  async _shouldReviveRemotelyDeletedRecord(remoteItem) {
    return true;
  },

  _deleteId(id) {
    this._tracker.removeChangedID(id);
    this._noteDeletedId(id);
  },

  // Marks an ID for deletion at the end of the sync.
  _noteDeletedId(id) {
    if (this._delete.ids == null)
      this._delete.ids = [id];
    else
      this._delete.ids.push(id);
  },

  async _switchItemToDupe(localDupeGUID, incomingItem) {
    // The local, duplicate ID is always deleted on the server.
    this._deleteId(localDupeGUID);

    // We unconditionally change the item's ID in case the engine knows of
    // an item but doesn't expose it through itemExists. If the API
    // contract were stronger, this could be changed.
    this._log.debug("Switching local ID to incoming: " + localDupeGUID + " -> " +
                    incomingItem.id);
    return this._store.changeItemID(localDupeGUID, incomingItem.id);
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
  async _reconcile(item) {
    if (this._log.level <= Log.Level.Trace) {
      this._log.trace("Incoming: " + item);
    }

    // We start reconciling by collecting a bunch of state. We do this here
    // because some state may change during the course of this function and we
    // need to operate on the original values.
    let existsLocally   = await this._store.itemExists(item.id);
    let locallyModified = this._modified.has(item.id);

    // TODO Handle clock drift better. Tracked in bug 721181.
    let remoteAge = Resource.serverTime - item.modified;
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
      let willRevive = await this._shouldReviveRemotelyDeletedRecord(item);
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
      let localDupeGUID = await this._findDupe(item);
      if (localDupeGUID) {
        this._log.trace("Local item " + localDupeGUID + " is a duplicate for " +
                        "incoming item " + item.id);

        // The current API contract does not mandate that the ID returned by
        // _findDupe() actually exists. Therefore, we have to perform this
        // check.
        existsLocally = await this._store.itemExists(localDupeGUID);

        // If the local item was modified, we carry its metadata forward so
        // appropriate reconciling can be performed.
        if (this._modified.has(localDupeGUID)) {
          locallyModified = true;
          localAge = this._tracker._now() - this._modified.getModifiedTimestamp(localDupeGUID);
          remoteIsNewer = remoteAge < localAge;

          this._modified.changeID(localDupeGUID, item.id);
        } else {
          locallyModified = false;
          localAge = null;
        }

        // Tell the engine to do whatever it needs to switch the items.
        await this._switchItemToDupe(localDupeGUID, item);

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
    let localRecord = await this._createRecord(item.id);
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
    if (!remoteIsNewer) {
      this.beforeRecordDiscard(localRecord, item, remoteIsNewer);
    }
    return remoteIsNewer;
  },

  // Upload outgoing records.
  async _uploadOutgoing() {
    this._log.trace("Uploading local changes to server.");

    // collection we'll upload
    let up = new Collection(this.engineURL, null, this.service);
    let modifiedIDs = new Set(this._modified.ids());
    for (let id of this._needWeakUpload.keys()) {
      modifiedIDs.add(id);
    }
    let counts = { failed: 0, sent: 0 };
    if (modifiedIDs.size) {
      this._log.trace("Preparing " + modifiedIDs.size +
                      " outgoing records");

      counts.sent = modifiedIDs.size;

      let failed = [];
      let successful = [];
      let handleResponse = async (resp, batchOngoing = false) => {
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

        await this._onRecordsWritten(successful, failed);

        // clear for next batch
        failed.length = 0;
        successful.length = 0;
      };

      let postQueue = up.newPostQueue(this._log, this.lastSync, handleResponse);

      for (let id of modifiedIDs) {
        let out;
        let ok = false;
        try {
          let { forceTombstone = false } = this._needWeakUpload.get(id) || {};
          if (forceTombstone) {
            out = await this._createTombstone(id);
          } else {
            out = await this._createRecord(id);
          }
          if (this._log.level <= Log.Level.Trace)
            this._log.trace("Outgoing: " + out);
          await out.encrypt(this.service.collectionKeys.keyForCollection(this.name));
          ok = true;
        } catch (ex) {
          this._log.warn("Error creating record", ex);
          ++counts.failed;
          if (Async.isShutdownException(ex) || !this.allowSkippedRecord) {
            if (!this.allowSkippedRecord) {
              // Don't bother for shutdown errors
              Observers.notify("weave:engine:sync:uploaded", counts, this.name);
            }
            throw ex;
          }
        }
        if (ok) {
          let { enqueued, error } = await postQueue.enqueue(out);
          if (!enqueued) {
            ++counts.failed;
            if (!this.allowSkippedRecord) {
              Observers.notify("weave:engine:sync:uploaded", counts, this.name);
              this._log.warn(`Failed to enqueue record "${id}" (aborting)`, error);
              throw error;
            }
            this._modified.delete(id);
            this._log.warn(`Failed to enqueue record "${id}" (skipping)`, error);
          }
        }
        await Async.promiseYield();
      }
      await postQueue.flush(true);
    }
    this._needWeakUpload.clear();

    if (counts.sent || counts.failed) {
      Observers.notify("weave:engine:sync:uploaded", counts, this.name);
    }
  },

  async _onRecordsWritten(succeeded, failed) {
    // Implement this method to take specific actions against successfully
    // uploaded records and failed records.
  },

  // Any cleanup necessary.
  // Save the current snapshot so as to calculate changes at next sync
  async _syncFinish() {
    this._log.trace("Finishing up sync");

    let doDelete = async (key, val) => {
      let coll = new Collection(this.engineURL, this._recordObj, this.service);
      coll[key] = val;
      await coll.delete();
    };

    for (let [key, val] of Object.entries(this._delete)) {
      // Remove the key for future uses
      delete this._delete[key];

      this._log.trace("doing post-sync deletions", {key, val});
      // Send a simple delete for the property
      if (key != "ids" || val.length <= 100)
        await doDelete(key, val);
      else {
        // For many ids, split into chunks of at most 100
        while (val.length > 0) {
          await doDelete(key, val.slice(0, 100));
          val = val.slice(100);
        }
      }
    }
  },

  async _syncCleanup() {
    this._needWeakUpload.clear();
    if (!this._modified) {
      return;
    }

    try {
      // Mark failed WBOs as changed again so they are reuploaded next time.
      await this.trackRemainingChanges();
    } finally {
      this._modified.clear();
    }
  },

  async _sync() {
    try {
      Async.checkAppReady();
      await this._syncStartup();
      Async.checkAppReady();
      Observers.notify("weave:engine:sync:status", "process-incoming");
      await this._processIncoming();
      Async.checkAppReady();
      Observers.notify("weave:engine:sync:status", "upload-outgoing");
      try {
        await this._uploadOutgoing();
        Async.checkAppReady();
        await this._syncFinish();
      } catch (ex) {
        if (!ex.status || ex.status != 412) {
          throw ex;
        }
        // a 412 posting just means another client raced - but we don't want
        // to treat that as a sync error - the next sync is almost certain
        // to work.
        this._log.warn("412 error during sync - will retry.");
      }
    } finally {
      await this._syncCleanup();
    }
  },

  async canDecrypt() {
    // Report failure even if there's nothing to decrypt
    let canDecrypt = false;

    // Fetch the most recently uploaded record and try to decrypt it
    let test = new Collection(this.engineURL, this._recordObj, this.service);
    test.limit = 1;
    test.sort = "newest";
    test.full = true;

    let key = this.service.collectionKeys.keyForCollection(this.name);

    // Any failure fetching/decrypting will just result in false
    try {
      this._log.trace("Trying to decrypt a record from the server..");
      let json = (await test.get()).obj[0];
      let record = new this._recordObj();
      record.deserialize(json);
      await record.decrypt(key);
      canDecrypt = true;
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        throw ex;
      }
      this._log.debug("Failed test decrypt", ex);
    }

    return canDecrypt;
  },

  async _resetClient() {
    this.resetLastSync();
    this.previousFailed = [];
    this.toFetch = [];
    this._needWeakUpload.clear();
  },

  async wipeServer() {
    let response = await this.service.resource(this.engineURL).delete();
    if (response.status != 200 && response.status != 404) {
      throw response;
    }
    await this._resetClient();
  },

  async removeClientData() {
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
  async handleHMACMismatch(item, mayRetry) {
    // By default we either try again, or bail out noisily.
    return ((await this.service.handleHMACEvent()) && mayRetry) ?
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
  async pullAllChanges() {
    let changes = {};
    let ids = await this._store.getAllIDs();
    for (let id in ids) {
      changes[id] = 0;
    }
    return changes;
  },

  /*
   * Returns a changeset containing entries for all currently tracked items.
   * The default implementation returns a changeset with timestamps indicating
   * when the item was added to the tracker.
   *
   * @return A `Changeset` object.
   */
  async pullNewChanges() {
    return this.getChangedIDs();
  },

  /**
   * Adds all remaining changeset entries back to the tracker, typically for
   * items that failed to upload. This method is called at the end of each sync.
   *
   */
  async trackRemainingChanges() {
    for (let [id, change] of this._modified.entries()) {
      this._tracker.addChangedID(id, change);
    }
  },

  async finalize() {
    await super.finalize();
    await this._toFetchStorage.finalize();
    await this._previousFailedStorage.finalize();
  },
};

/**
 * A changeset is created for each sync in `Engine::get{Changed, All}IDs`,
 * and stores opaque change data for tracked IDs. The default implementation
 * only records timestamps, though engines can extend this to store additional
 * data for each entry.
 */
class Changeset {
  // Creates an empty changeset.
  constructor() {
    this.changes = {};
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

  // Adds multiple entries to the changeset, preserving existing entries.
  insert(changes) {
    Object.assign(this.changes, changes);
  }

  // Overwrites the existing set of tracked changes with new entries.
  replace(changes) {
    this.changes = changes;
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

  // Changes the ID of an entry in the changeset. Used when reconciling
  // duplicates that have local changes.
  changeID(oldID, newID) {
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
