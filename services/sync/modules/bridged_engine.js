/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file has all the machinery for hooking up bridged engines implemented
 * in Rust. It's the JavaScript side of the Golden Gate bridge that connects
 * Desktop Sync to a Rust `BridgedEngine`, via the `mozIBridgedSyncEngine`
 * XPCOM interface.
 *
 * Creating a bridged engine only takes a few lines of code, since most of the
 * hard work is done on the Rust side. On the JS side, you'll need to subclass
 * `BridgedEngine` (instead of `SyncEngine`), supply a `mozIBridgedSyncEngine`
 * for your subclass to wrap, and optionally implement and override the tracker.
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Changeset, SyncEngine, Tracker } = ChromeUtils.import(
  "resource://services-sync/engines.js"
);
const { RawCryptoWrapper } = ChromeUtils.import(
  "resource://services-sync/record.js"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Log: "resource://gre/modules/Log.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

var EXPORTED_SYMBOLS = ["BridgedEngine", "LogAdapter"];

/**
 * A stub store that converts between raw decrypted incoming records and
 * envelopes. Since the interface we need is so minimal, this class doesn't
 * inherit from the base `Store` implementation...it would take more code to
 * override all those behaviors!
 *
 * This class isn't meant to be subclassed, because bridged engines shouldn't
 * override their store classes in `_storeObj`.
 */
class BridgedStore {
  constructor(name, engine) {
    if (!engine) {
      throw new Error("Store must be associated with an Engine instance.");
    }
    this.engine = engine;
    this._log = lazy.Log.repository.getLogger(`Sync.Engine.${name}.Store`);
    this._batchChunkSize = 500;
  }

  async applyIncomingBatch(records) {
    for (let chunk of lazy.PlacesUtils.chunkArray(
      records,
      this._batchChunkSize
    )) {
      let incomingEnvelopesAsJSON = chunk.map(record =>
        JSON.stringify(record.toIncomingEnvelope())
      );
      this._log.trace("incoming envelopes", incomingEnvelopesAsJSON);
      await promisify(
        this.engine._bridge.storeIncoming,
        incomingEnvelopesAsJSON
      );
    }
    // Array of failed records.
    return [];
  }

  async wipe() {
    await promisify(this.engine._bridge.wipe);
  }
}

/**
 * A wrapper class to convert between BSOs on the JS side, and envelopes on the
 * Rust side. This class intentionally subclasses `RawCryptoWrapper`, because we
 * don't want the stringification and parsing machinery in `CryptoWrapper`.
 *
 * This class isn't meant to be subclassed, because bridged engines shouldn't
 * override their record classes in `_recordObj`.
 */
class BridgedRecord extends RawCryptoWrapper {
  /**
   * Creates an outgoing record from an envelope returned by a bridged engine.
   * This must be kept in sync with `sync15_traits::OutgoingEnvelope`.
   *
   * @param  {String} collection The collection name.
   * @param  {Object} envelope   The outgoing envelope, returned from
   *                             `mozIBridgedSyncEngine::apply`.
   * @return {BridgedRecord}     A Sync record ready to encrypt and upload.
   */
  static fromOutgoingEnvelope(collection, envelope) {
    if (typeof envelope.id != "string") {
      throw new TypeError("Outgoing envelope missing ID");
    }
    if (typeof envelope.cleartext != "string") {
      throw new TypeError("Outgoing envelope missing cleartext");
    }
    let record = new BridgedRecord(collection, envelope.id);
    record.cleartext = envelope.cleartext;
    return record;
  }

  transformBeforeEncrypt(cleartext) {
    if (typeof cleartext != "string") {
      throw new TypeError("Outgoing bridged engine records must be strings");
    }
    return cleartext;
  }

  transformAfterDecrypt(cleartext) {
    if (typeof cleartext != "string") {
      throw new TypeError("Incoming bridged engine records must be strings");
    }
    return cleartext;
  }

  /*
   * Converts this incoming record into an envelope to pass to a bridged engine.
   * This object must be kept in sync with `sync15_traits::IncomingEnvelope`.
   *
   * @return {Object} The incoming envelope, to pass to
   *                  `mozIBridgedSyncEngine::storeIncoming`.
   */
  toIncomingEnvelope() {
    return {
      id: this.data.id,
      modified: this.data.modified,
      cleartext: this.cleartext,
    };
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
 * Adapts a `Log.jsm` logger to a `mozIServicesLogSink`. This class is copied
 * from `SyncedBookmarksMirror.jsm`.
 */
class LogAdapter {
  constructor(log) {
    this.log = log;
  }

  get maxLevel() {
    let level = this.log.level;
    if (level <= lazy.Log.Level.All) {
      return Ci.mozIServicesLogSink.LEVEL_TRACE;
    }
    if (level <= lazy.Log.Level.Info) {
      return Ci.mozIServicesLogSink.LEVEL_DEBUG;
    }
    if (level <= lazy.Log.Level.Warn) {
      return Ci.mozIServicesLogSink.LEVEL_WARN;
    }
    if (level <= lazy.Log.Level.Error) {
      return Ci.mozIServicesLogSink.LEVEL_ERROR;
    }
    return Ci.mozIServicesLogSink.LEVEL_OFF;
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
 * A base class used to plug a Rust engine into Sync, and have it work like any
 * other engine. The constructor takes a bridge as its first argument, which is
 * an instance of an XPCOM component class that implements
 * `mozIBridgedSyncEngine`.
 *
 * This class inherits from `SyncEngine`, which has a lot of machinery that we
 * don't need, but that's fairly easy to override. It would be harder to
 * reimplement the machinery that we _do_ need here. However, because of that,
 * this class has lots of methods that do nothing, or return empty data. The
 * docs above each method explain what it's overriding, and why.
 *
 * This class is designed to be subclassed, but the only part that your engine
 * may want to override is `_trackerObj`. Even then, using the default (no-op)
 * tracker is fine, because the shape of the `Tracker` interface may not make
 * sense for all engines.
 */
function BridgedEngine(bridge, name, service) {
  SyncEngine.call(this, name, service);

  this._bridge = bridge;
}

BridgedEngine.prototype = {
  __proto__: SyncEngine.prototype,

  /**
   * The tracker class for this engine. Subclasses may want to override this
   * with their own tracker, though using the default `Tracker` is fine.
   */
  _trackerObj: Tracker,

  /** Returns the record class for all bridged engines. */
  get _recordObj() {
    return BridgedRecord;
  },

  set _recordObj(obj) {
    throw new TypeError("Don't override the record class for bridged engines");
  },

  /** Returns the store class for all bridged engines. */
  get _storeObj() {
    return BridgedStore;
  },

  set _storeObj(obj) {
    throw new TypeError("Don't override the store class for bridged engines");
  },

  /** Returns the storage version for this engine. */
  get version() {
    return this._bridge.storageVersion;
  },

  // Legacy engines allow sync to proceed if some records are too large to
  // upload (eg, a payload that's bigger than the server's published limits).
  // If this returns true, we will just skip the record without even attempting
  // to upload. If this is false, we'll abort the entire batch.
  // If the engine allows this, it will need to detect this scenario by noticing
  // the ID is not in the 'success' records reported to `setUploaded`.
  // (Note that this is not to be confused with the fact server's can currently
  // reject records as part of a POST - but we hope to remove this ability from
  // the server API. Note also that this is not bullet-proof - if the count of
  // records is high, it's possible that we will have committed a previous
  // batch before we hit the relevant limits, so things might have been written.
  // We hope to fix this by ensuring batch limits are such that this is
  // impossible)
  get allowSkippedRecord() {
    return this._bridge.allowSkippedRecord;
  },

  /**
   * Returns the sync ID for this engine. This is exposed for tests, but
   * Sync code always calls `resetSyncID()` and `ensureCurrentSyncID()`,
   * not this.
   *
   * @returns {String?} The sync ID, or `null` if one isn't set.
   */
  async getSyncID() {
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
    let newSyncID = await promisify(this._bridge.resetSyncId);
    return newSyncID;
  },

  async ensureCurrentSyncID(newSyncID) {
    let assignedSyncID = await promisify(
      this._bridge.ensureCurrentSyncId,
      newSyncID
    );
    return assignedSyncID;
  },

  async getLastSync() {
    // The bridge defines lastSync as integer ms, but sync itself wants to work
    // in a float seconds with 2 decimal places.
    let lastSyncMS = await promisify(this._bridge.getLastSync);
    return Math.round(lastSyncMS / 10) / 100;
  },

  async setLastSync(lastSyncSeconds) {
    await promisify(
      this._bridge.setLastSync,
      Math.round(lastSyncSeconds * 1000)
    );
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
    await promisify(this._bridge.syncFinished);
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

  async _syncStartup() {
    await super._syncStartup();
    await promisify(this._bridge.syncStarted);
  },

  async _processIncoming(newitems) {
    await super._processIncoming(newitems);

    let outgoingEnvelopesAsJSON = await promisify(this._bridge.apply);
    let changeset = {};
    for (let envelopeAsJSON of outgoingEnvelopesAsJSON) {
      this._log.trace("outgoing envelope", envelopeAsJSON);
      let record = BridgedRecord.fromOutgoingEnvelope(
        this.name,
        JSON.parse(envelopeAsJSON)
      );
      changeset[record.id] = {
        synced: false,
        record,
      };
    }
    this._modified.replace(changeset);
  },

  /**
   * Notify the bridged engine that we've successfully uploaded a batch, so
   * that it can update its local state. For example, if the engine uses a
   * mirror and a temp table for outgoing records, it can write the uploaded
   * records from the outgoing table back to the mirror.
   */
  async _onRecordsWritten(succeeded, failed, serverModifiedTime) {
    await promisify(this._bridge.setUploaded, serverModifiedTime, succeeded);
  },

  async _createTombstone() {
    throw new Error("Bridged engines don't support weak uploads");
  },

  async _createRecord(id) {
    let change = this._modified.changes[id];
    if (!change) {
      throw new TypeError("Can't create record for unchanged item");
    }
    return change.record;
  },

  async _resetClient() {
    await super._resetClient();
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
