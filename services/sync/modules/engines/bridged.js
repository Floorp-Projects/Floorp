/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Changeset, SyncEngine } = ChromeUtils.import(
  "resource://services-sync/engines.js"
);
const { CryptoWrapper } = ChromeUtils.import(
  "resource://services-sync/record.js"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Async: "resource://services-common/async.js",
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

var EXPORTED_SYMBOLS = [
  "BridgedEngine",
  "BridgedStore",
  "BridgedTracker",
  "BridgedRecord",
];

/**
 * A stub store that keeps all decrypted records in memory. Since the interface
 * we need is so minimal, this class doesn't inherit from the base `Store`
 * implementation...it would take more code to override all those behaviors!
 */
class BridgedStore {
  constructor(name, engine) {
    if (!engine) {
      throw new Error("Store must be associated with an Engine instance.");
    }
    this.engine = engine;
    this._log = Log.repository.getLogger(`Sync.Engine.${name}.Store`);
    this._batchChunkSize = 500;
  }

  async applyIncomingBatch(records) {
    await this.engine.initialize();
    for (let chunk of PlacesUtils.chunkArray(records, this._batchChunkSize)) {
      // TODO: We can avoid parsing and re-serializing here... We also need to
      // pass attributes like `modified` and `sortindex`, which are not part
      // of the cleartext.
      let incomingCleartexts = chunk.map(record => record.cleartextToString());
      await promisifyWithSignal(
        null,
        this.engine._bridge.storeIncoming,
        incomingCleartexts
      );
    }
    // Array of failed records.
    return [];
  }

  async wipe() {
    await this.engine.initialize();
    await promisify(this.engine._bridge.wipe);
  }
}

/**
 * A stub tracker that doesn't track anything.
 */
class BridgedTracker {
  constructor(name, engine) {
    if (!engine) {
      throw new Error("Tracker must be associated with an Engine instance.");
    }

    this._log = Log.repository.getLogger(`Sync.Engine.${name}.Tracker`);
    this.score = 0;
    this.asyncObserver = Async.asyncObserver(this, this._log);
  }

  get ignoreAll() {
    return false;
  }

  set ignoreAll(value) {}

  async onEngineEnabledChanged(engineEnabled) {
    // ...
  }

  resetScore() {
    this.score = 0;
  }

  start() {
    // ...
  }

  async stop() {
    // ...
  }

  async clearChangedIDs() {
    // ...
  }

  async finalize() {
    // ...
  }
}

class BridgedRecord extends CryptoWrapper {
  constructor(collection, id, type) {
    super(collection, id, type);
  }
}

class BridgeError extends Error {
  constructor(code, message) {
    super(message);
    this.name = "BridgeError";
    // TODO: We may want to use a different name for this, since errors with
    // a `result` property are treated specially by telemetry, discarding the
    // message...but, unlike other `nserror`s, the message is actually useful,
    // and we still want to capture it.
    this.result = code;
  }
}

class InterruptedError extends Error {
  constructor(message) {
    super(message);
    this.name = "InterruptedError";
  }
}

/**
 * Adapts a `Log.jsm` logger to a `mozIServicesLogger`. This class is copied
 * from `SyncedBookmarksMirror.jsm`.
 */
class LogAdapter {
  constructor(log) {
    this.log = log;
  }

  get maxLevel() {
    let level = this.log.level;
    if (level <= Log.Level.All) {
      return Ci.mozIServicesLogger.LEVEL_TRACE;
    }
    if (level <= Log.Level.Info) {
      return Ci.mozIServicesLogger.LEVEL_DEBUG;
    }
    if (level <= Log.Level.Warn) {
      return Ci.mozIServicesLogger.LEVEL_WARN;
    }
    if (level <= Log.Level.Error) {
      return Ci.mozIServicesLogger.LEVEL_ERROR;
    }
    return Ci.mozIServicesLogger.LEVEL_OFF;
  }

  trace(message) {
    this.log.trace(message);
  }

  debug(message) {
    this.log.debug(message);
  }

  warn(message) {
    this.log.warn(message);
  }

  error(message) {
    this.log.error(message);
  }
}

/**
 * The JavaScript side of the native bridge. This is a base class that can be
 * used to wire up a Sync engine written in Rust to the existing Sync codebase,
 * and have it work like any other engine. The Rust side must expose an XPCOM
 * component class that implements the `mozIBridgedSyncEngine` interface.
 *
 * `SyncEngine` has a lot of machinery that we don't need, but makes it fairly
 * easy to opt out by overriding those methods. It would be harder to
 * reimplement the machinery that we _do_ need here, especially for a first cut.
 * However, because of that, this class has lots of methods that do nothing, or
 * return empty data. The docs above each method explain what it's overriding,
 * and why.
 */
function BridgedEngine(bridge, name, service) {
  SyncEngine.call(this, name, service);

  this._bridge = bridge;
  this._bridge.logger = new LogAdapter(this._log);

  // The maximum amount of time that we should wait for the bridged engine
  // to apply incoming records before aborting.
  this._applyTimeoutMillis = 5 * 60 * 60 * 1000; // 5 minutes
}

BridgedEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: BridgedRecord,
  _storeObj: BridgedStore,
  _trackerObj: BridgedTracker,

  _initializePromise: null,

  /** Returns the storage version for this engine. */
  get version() {
    return this._bridge.storageVersion;
  },

  // Legacy engines allow sync to proceed if some records fail to upload. Since
  // we've supported batch uploads on our server for a while, and we want to
  // make them stricter (for example, failing the entire batch if a record can't
  // be stored, instead of returning its ID in the `failed` response field), we
  // require all bridged engines to opt out of partial syncs.
  get allowSkippedRecord() {
    return false;
  },

  /**
   * Initializes the underlying Rust bridge for this engine. Once the bridge is
   * ready, subsequent calls to `initialize` are no-ops. If initialization
   * fails, the next call to `initialize` will try again.
   *
   * @throws  If initializing the bridge fails.
   */
  async initialize() {
    if (!this._initializePromise) {
      this._initializePromise = promisify(this._bridge.initialize).catch(
        err => {
          // We may have failed to initialize the bridge temporarily; for example,
          // if its database is corrupt. Clear the promise so that subsequent
          // calls to `initialize` can try to create the bridge again.
          this._initializePromise = null;
          throw err;
        }
      );
    }
    return this._initializePromise;
  },

  /**
   * Returns the sync ID for this engine. This is exposed for tests, but
   * Sync code always calls `resetSyncID()` and `ensureCurrentSyncID()`,
   * not this.
   *
   * @returns {String?} The sync ID, or `null` if one isn't set.
   */
  async getSyncID() {
    await this.initialize();
    // Note that all methods on an XPCOM class instance are automatically bound,
    // so we don't need to write `this._bridge.getSyncId.bind(this._bridge)`.
    let syncID = await promisify(this._bridge.getSyncId);
    return syncID;
  },

  async resetSyncID() {
    await this._deleteServerCollection();
    let newSyncID = await this.resetLocalSyncID();
    return newSyncID;
  },

  async resetLocalSyncID() {
    await this.initialize();
    let newSyncID = await promisify(this._bridge.resetSyncId);
    return newSyncID;
  },

  async ensureCurrentSyncID(newSyncID) {
    await this.initialize();
    let assignedSyncID = await promisify(
      this._bridge.ensureCurrentSyncId,
      newSyncID
    );
    return assignedSyncID;
  },

  async getLastSync() {
    await this.initialize();
    let lastSync = await promisify(this._bridge.getLastSync);
    return lastSync;
  },

  async setLastSync(lastSyncMillis) {
    await this.initialize();
    await promisify(this._bridge.setLastSync, lastSyncMillis);
  },

  /**
   * Returns the initial changeset for the sync. Bridged engines handle
   * reconciliation internally, so we don't know what changed until after we've
   * stored and applied all incoming records. So we return an empty changeset
   * here, and replace it with the real one in `_processIncoming`.
   */
  async pullChanges() {
    return {};
  },

  async trackRemainingChanges() {
    // TODO: Should we call `storeIncoming` here again, to write the records we
    // just uploaded (that is, records in the changeset where `synced = true`)
    // back to the bridged engine's mirror? Or can we rely on the engine to
    // keep the records around (for example, in a temp table), and automatically
    // write them back on `syncFinished`?
    await this.initialize();
    await promisifyWithSignal(null, this._bridge.syncFinished);
  },

  /**
   * Marks a record for a hard-`DELETE` at the end of the sync. The base method
   * also removes it from the tracker, but we don't use the tracker for that,
   * so we override the method to just mark.
   */
  _deleteId(id) {
    this._noteDeletedId(id);
  },

  /**
   * Always stage incoming records, bypassing the base engine's reconciliation
   * machinery.
   */
  async _reconcile() {
    return true;
  },

  async _processIncoming(newitems) {
    await super._processIncoming(newitems);
    await this.initialize();

    // TODO: We could consider having a per-sync watchdog instead; for
    // example, time out after 5 minutes total, including any network
    // latency. `promisifyWithSignal` makes this flexible.
    let watchdog = this._newWatchdog();
    watchdog.start(this._applyTimeoutMillis);

    try {
      let outgoingRecords = await promisifyWithSignal(
        watchdog.signal,
        this._bridge.apply
      );
      let changeset = {};
      for (let record of outgoingRecords) {
        // TODO: It would be nice if we could pass the cleartext through as-is
        // here, too, instead of parsing and re-wrapping for `BridgedRecord`.
        let cleartext = JSON.parse(record);
        changeset[cleartext.id] = {
          synced: false,
          cleartext,
        };
      }
      this._modified.replace(changeset);
    } finally {
      watchdog.stop();
      if (watchdog.abortReason) {
        this._log.warn(`Aborting bookmark merge: ${watchdog.abortReason}`);
      }
    }
  },

  /**
   * Notify the bridged engine that we've successfully uploaded a batch, so
   * that it can update its local state. For example, if the engine uses a
   * mirror and a temp table for outgoing records, it can write the uploaded
   * records from the outgoing table back to the mirror.
   */
  async _onRecordsWritten(succeeded, failed, serverModifiedTime) {
    await this.initialize();
    await promisifyWithSignal(
      null,
      this._bridge.setUploaded,
      serverModifiedTime,
      succeeded
    );
  },

  async _createTombstone() {
    throw new Error("Bridged engines don't support weak uploads");
  },

  async _createRecord(id) {
    let change = this._modified.changes[id];
    if (!change) {
      throw new TypeError("Can't create record for unchanged item");
    }
    let record = new this._recordObj(this.name, id);
    record.cleartext = change.cleartext;
    return record;
  },

  async _resetClient() {
    await super._resetClient();
    await this.initialize();
    await promisify(this._bridge.reset);
  },
};

function transformError(code, message) {
  switch (code) {
    case Cr.NS_ERROR_ABORT:
      return new InterruptedError(message);

    default:
      return new BridgeError(code, message);
  }
}

// Converts a bridged function that takes a callback into one that returns a
// promise.
function promisify(func, ...params) {
  return new Promise((resolve, reject) => {
    func(...params, {
      // This object implicitly implements all three callback interfaces
      // (`mozIBridgedSyncEngine{Apply, Result}Callback`), because they have
      // the same methods. The only difference is the type of the argument
      // passed to `handleSuccess`, which doesn't matter in JS.
      handleSuccess: resolve,
      handleError(code, message) {
        reject(transformError(code, message));
      },
    });
  });
}

// Like `promisify`, but takes an `AbortSignal` for cancelable
// operations.
function promisifyWithSignal(signal, func, ...params) {
  if (!signal) {
    return promisify(func, ...params);
  }
  return new Promise((resolve, reject) => {
    if (signal.aborted) {
      // TODO: Record more specific operation names, so we can see which
      // ones get interrupted most in telemetry.
      throw new InterruptedError("Interrupted before starting operation");
    }
    function onAbort() {
      signal.removeEventListener("abort", onAbort);
      op.cancel(Cr.NS_ERROR_ABORT);
    }
    let op = func(...params, {
      handleSuccess(result) {
        signal.removeEventListener("abort", onAbort);
        resolve(result);
      },
      handleError(code, message) {
        reject(transformError(code, message));
      },
    });
    signal.addEventListener("abort", onAbort);
  });
}

class BridgedChangeset extends Changeset {
  // Only `_reconcile` calls `getModifiedTimestamp` and `has`, and the buffered
  // engine does its own reconciliation.
  getModifiedTimestamp(id) {
    throw new Error("Don't use timestamps to resolve bridged engine conflicts");
  }

  has(id) {
    throw new Error(
      "Don't use the changeset to resolve bridged engine conflicts"
    );
  }

  delete(id) {
    let change = this.changes[id];
    if (change) {
      // Mark the change as synced without removing it from the set. Depending
      // on how we implement `trackRemainingChanges`, this may not be necessary.
      // It's copied from the bookmarks changeset now.
      change.synced = true;
    }
  }

  ids() {
    let results = [];
    for (let id in this.changes) {
      if (!this.changes[id].synced) {
        results.push(id);
      }
    }
    return results;
  }
}
