/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "RemoteSettingsClient",
];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Kinto",
                               "resource://services-common/kinto-offline-client.js");
ChromeUtils.defineModuleGetter(this, "KintoHttpClient",
                               "resource://services-common/kinto-http-client.js");
ChromeUtils.defineModuleGetter(this, "UptakeTelemetry",
                               "resource://services-common/uptake-telemetry.js");
ChromeUtils.defineModuleGetter(this, "ClientEnvironmentBase",
                               "resource://gre/modules/components-utils/ClientEnvironment.jsm");
ChromeUtils.defineModuleGetter(this, "RemoteSettingsWorker",
                               "resource://services-settings/RemoteSettingsWorker.jsm");
ChromeUtils.defineModuleGetter(this, "Utils",
                               "resource://services-settings/Utils.jsm");
ChromeUtils.defineModuleGetter(this, "Downloader",
                               "resource://services-settings/Attachments.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

// IndexedDB name.
const DB_NAME = "remote-settings";

const TELEMETRY_COMPONENT = "remotesettings";

XPCOMUtils.defineLazyPreferenceGetter(this, "gServerURL",
                                      "services.settings.server");

/**
 * cacheProxy returns an object Proxy that will memoize properties of the target.
 * @param {Object} target the object to wrap.
 * @returns {Proxy}
 */
function cacheProxy(target) {
  const cache = new Map();
  return new Proxy(target, {
    get(target, prop, receiver) {
      if (!cache.has(prop)) {
        cache.set(prop, target[prop]);
      }
      return cache.get(prop);
    },
  });
}

class ClientEnvironment extends ClientEnvironmentBase {
  static get appID() {
    // eg. Firefox is "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}".
    Services.appinfo.QueryInterface(Ci.nsIXULAppInfo);
    return Services.appinfo.ID;
  }

  static get toolkitVersion() {
    Services.appinfo.QueryInterface(Ci.nsIPlatformInfo);
    return Services.appinfo.platformVersion;
  }
}

/**
 * Minimalist event emitter.
 *
 * Note: we don't use `toolkit/modules/EventEmitter` because **we want** to throw
 * an error when a listener fails to execute.
 */
class EventEmitter {
  constructor(events) {
    this._listeners = new Map();
    for (const event of events) {
      this._listeners.set(event, []);
    }
  }

  /**
   * Event emitter: will execute the registered listeners in the order and
   * sequentially.
   *
   * @param {string} event    the event name
   * @param {Object} payload  the event payload to call the listeners with
   */
  async emit(event, payload) {
    const callbacks = this._listeners.get(event);
    let lastError;
    for (const cb of callbacks) {
      try {
        await cb(payload);
      } catch (e) {
        lastError = e;
      }
    }
    if (lastError) {
      throw lastError;
    }
  }

  on(event, callback) {
    if (!this._listeners.has(event)) {
      throw new Error(`Unknown event type ${event}`);
    }
    this._listeners.get(event).push(callback);
  }

  off(event, callback) {
    if (!this._listeners.has(event)) {
      throw new Error(`Unknown event type ${event}`);
    }
    const callbacks = this._listeners.get(event);
    const i = callbacks.indexOf(callback);
    if (i < 0) {
      throw new Error(`Unknown callback`);
    } else {
      callbacks.splice(i, 1);
    }
  }
}

class InvalidSignatureError extends Error {
  constructor(cid) {
    super(`Invalid content signature (${cid})`);
    this.name = "InvalidSignatureError";
  }
}

class MissingSignatureError extends Error {
  constructor(cid) {
    super(`Missing signature (${cid})`);
    this.name = "MissingSignatureError";
  }
}

class RemoteSettingsClient extends EventEmitter {
  static get InvalidSignatureError() { return InvalidSignatureError; }
  static get MissingSignatureError() { return MissingSignatureError; }

  constructor(collectionName, { bucketNamePref, signerName, filterFunc, localFields = [], lastCheckTimePref }) {
    super(["sync"]); // emitted events

    this.collectionName = collectionName;
    this.signerName = signerName;
    this.filterFunc = filterFunc;
    this.localFields = localFields;
    this._lastCheckTimePref = lastCheckTimePref;
    this._verifier = null;

    // This attribute allows signature verification to be disabled, when running tests
    // or when pulling data from a dev server.
    this.verifySignature = true;

    // The bucket preference value can be changed (eg. `main` to `main-preview`) in order
    // to preview the changes to be approved in a real client.
    this.bucketNamePref = bucketNamePref;
    XPCOMUtils.defineLazyPreferenceGetter(this, "bucketName", this.bucketNamePref);

    XPCOMUtils.defineLazyGetter(this, "_kinto", () => new Kinto({
      bucket: this.bucketName,
      adapter: Kinto.adapters.IDB,
      adapterOptions: { dbName: DB_NAME, migrateOldData: false },
    }));

    XPCOMUtils.defineLazyGetter(this, "attachments", () => new Downloader(this.bucketName, collectionName));
  }

  get identifier() {
    return `${this.bucketName}/${this.collectionName}`;
  }

  get lastCheckTimePref() {
    return this._lastCheckTimePref || `services.settings.${this.bucketName}.${this.collectionName}.last_check`;
  }

  /**
   * Open the underlying Kinto collection, using the appropriate adapter and options.
   */
  async openCollection() {
    const options = {
      localFields: this.localFields,
      bucket: this.bucketName,
    };
    return this._kinto.collection(this.collectionName, options);
  }

  /**
   * Lists settings.
   *
   * @param  {Object} options                  The options object.
   * @param  {Object} options.filters          Filter the results (default: `{}`).
   * @param  {String} options.order            The order to apply (eg. `"-last_modified"`).
   * @param  {boolean} options.syncIfEmpty     Synchronize from server if local data is empty (default: `true`).
   * @param  {boolean} options.verifySignature Verify the signature of the local data (default: `false`).
   * @return {Promise}
   */
  async get(options = {}) {
    const {
      filters = {},
      order = "", // not sorted by default.
      syncIfEmpty = true,
    } = options;
    let { verifySignature = false } = options;

    if (syncIfEmpty && !(await Utils.hasLocalData(this))) {
      try {
        // .get() was called before we had the chance to synchronize the local database.
        // We'll try to avoid returning an empty list.
        if (await Utils.hasLocalDump(this.bucketName, this.collectionName)) {
          // Since there is a JSON dump, load it as default data.
          await RemoteSettingsWorker.importJSONDump(this.bucketName, this.collectionName);
        } else {
          // There is no JSON dump, force a synchronization from the server.
          await this.sync({ loadDump: false });
        }
        // Either from trusted dump, or already done during sync.
        verifySignature = false;
      } catch (e) {
        // Report but return an empty list since there will be no data anyway.
        Cu.reportError(e);
        return [];
      }
    }

    // Read from the local DB.
    const kintoCollection = await this.openCollection();
    const { data } = await kintoCollection.list({ filters, order });

    // Verify signature of local data.
    if (verifySignature) {
      const localRecords = data.map(r => kintoCollection.cleanLocalFields(r));
      const timestamp = await kintoCollection.db.getLastModified();
      const metadata = await kintoCollection.metadata();
      await this._validateCollectionSignature([],
                                              timestamp,
                                              metadata,
                                              { localRecords });
    }

    // Filter the records based on `this.filterFunc` results.
    return this._filterEntries(data);
  }

  /**
   * Synchronize the local database with the remote server.
   *
   * @param {Object} options See #maybeSync() options.
   */
  async sync(options) {
    // We want to know which timestamp we are expected to obtain in order to leverage
    // cache busting. We don't provide ETag because we don't want a 304.
    const { changes } = await Utils.fetchLatestChanges(gServerURL, {
      filters: {
        collection: this.collectionName,
        bucket: this.bucketName,
      },
    });
    if (changes.length === 0) {
      throw new Error(`Unknown collection "${this.identifier}"`);
    }
    // According to API, there will be one only (fail if not).
    const [{ last_modified: expectedTimestamp }] = changes;

    return this.maybeSync(expectedTimestamp, { ...options, trigger: "forced" });
  }

  /**
   * Synchronize the local database with the remote server, **only if necessary**.
   *
   * @param {int}    expectedTimestamp the lastModified date (on the server) for the remote collection.
   *                                   This will be compared to the local timestamp, and will be used for
   *                                   cache busting if local data is out of date.
   * @param {Object} options           additional advanced options.
   * @param {bool}   options.loadDump  load initial dump from disk on first sync (default: true)
   * @param {string} options.trigger   label to identify what triggered this sync (eg. ``"timer"``, default: `"manual"`)
   * @return {Promise}                 which rejects on sync or process failure.
   */
  async maybeSync(expectedTimestamp, options = {}) {
    const { loadDump = true, trigger = "manual" } = options;

    const startedAt = new Date();
    let reportStatus = null;
    try {
      // Synchronize remote data into a local DB using Kinto.
      const kintoCollection = await this.openCollection();
      let collectionLastModified = await kintoCollection.db.getLastModified();

      // If there is no data currently in the collection, attempt to import
      // initial data from the application defaults.
      // This allows to avoid synchronizing the whole collection content on
      // cold start.
      if (!collectionLastModified && loadDump) {
        try {
          await RemoteSettingsWorker.importJSONDump(this.bucketName, this.collectionName);
          collectionLastModified = await kintoCollection.db.getLastModified();
        } catch (e) {
          // Report but go-on.
          Cu.reportError(e);
        }
      }

      // If the data is up to date, there's no need to sync. We still need
      // to record the fact that a check happened.
      if (expectedTimestamp <= collectionLastModified) {
        reportStatus = UptakeTelemetry.STATUS.UP_TO_DATE;
        return;
      }

      // If signature verification is enabled, then add a synchronization hook
      // for incoming changes that validates the signature.
      if (this.verifySignature) {
        kintoCollection.hooks["incoming-changes"] = [async (payload, collection) => {
          const { changes: remoteRecords, lastModified: timestamp } = payload;
          const { data } = await kintoCollection.list({ order: "" }); // no need to sort.
          const metadata = await collection.metadata();
          // Local fields are stripped to compute the collection signature (server does not have them).
          const localRecords = data.map(r => kintoCollection.cleanLocalFields(r));
          await this._validateCollectionSignature(remoteRecords,
                                                  timestamp,
                                                  metadata,
                                                  { localRecords });
          // In case the signature is valid, apply the changes locally.
          return payload;
        }];
      }

      let syncResult;
      try {
        // Fetch changes from server, and make sure we overwrite local data.
        const strategy = Kinto.syncStrategy.SERVER_WINS;
        syncResult = await kintoCollection.sync({ remote: gServerURL, strategy, expectedTimestamp });
        if (!syncResult.ok) {
          // With SERVER_WINS, there cannot be any conflicts, but don't silent it anyway.
          throw new Error("Synced failed");
        }
      } catch (e) {
        if (e instanceof RemoteSettingsClient.InvalidSignatureError) {
          // Signature verification failed during synchronization.
          reportStatus = UptakeTelemetry.STATUS.SIGNATURE_ERROR;
          // If sync fails with a signature error, it's likely that our
          // local data has been modified in some way.
          // We will attempt to fix this by retrieving the whole
          // remote collection.
          try {
            syncResult = await this._retrySyncFromScratch(kintoCollection, expectedTimestamp);
          } catch (e) {
            // If the signature fails again, or if an error occured during wiping out the
            // local data, then we report it as a *signature retry* error.
            reportStatus = UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR;
            throw e;
          }
        } else {
          // The sync has thrown, it can be related to metadata, network or a general error.
          if (e instanceof RemoteSettingsClient.MissingSignatureError) {
            // Collection metadata has no signature info, no need to retry.
            reportStatus = UptakeTelemetry.STATUS.SIGNATURE_ERROR;
          } else if (/unparseable/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.PARSE_ERROR;
          } else if (/NetworkError/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.NETWORK_ERROR;
          } else if (/Timeout/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.TIMEOUT_ERROR;
          } else if (/HTTP 5??/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.SERVER_ERROR;
          } else if (/Backoff/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.BACKOFF;
          } else {
            reportStatus = UptakeTelemetry.STATUS.SYNC_ERROR;
          }
          throw e;
        }
      }
      // Filter the synchronization results using `filterFunc` (ie. JEXL).
      const filteredSyncResult = await this._filterSyncResult(kintoCollection, syncResult);
      // If every changed entry is filtered, we don't even fire the event.
      if (filteredSyncResult) {
        try {
          await this.emit("sync", { data: filteredSyncResult });
        } catch (e) {
          reportStatus = UptakeTelemetry.STATUS.APPLY_ERROR;
          throw e;
        }
      }
    } catch (e) {
      // IndexedDB errors. See https://developer.mozilla.org/en-US/docs/Web/API/IDBRequest/error
      if (/(IndexedDB|AbortError|ConstraintError|QuotaExceededError|VersionError)/.test(e.message)) {
        reportStatus = UptakeTelemetry.STATUS.CUSTOM_1_ERROR;
      }
      // No specific error was tracked, mark it as unknown.
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      throw e;
    } finally {
      const durationMilliseconds = new Date() - startedAt;
      // No error was reported, this is a success!
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.SUCCESS;
      }
      // Report success/error status to Telemetry.
      await UptakeTelemetry.report(TELEMETRY_COMPONENT, reportStatus, {
        source: this.identifier,
        trigger,
        duration: durationMilliseconds,
      });
    }
  }

  /**
   * Fetch the signature info from the collection metadata and verifies that the
   * local set of records has the same.
   *
   * @param {Array<Object>} remoteRecords   The list of changes to apply to the local database.
   * @param {int} timestamp                 The timestamp associated with the list of remote records.
   * @param {Object} metadata               The collection metadata, that contains the signature payload.
   * @param {Object} options
   * @param {Array<Object>} options.localRecords List of additional local records to take into account (default: `[]`).
   * @returns {Promise}
   */
  async _validateCollectionSignature(remoteRecords, timestamp, metadata, options = {}) {
    const { localRecords = [] } = options;

    if (!metadata || !metadata.signature) {
      throw new RemoteSettingsClient.MissingSignatureError(this.identifier);
    }

    if (!this._verifier) {
        this._verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
          .createInstance(Ci.nsIContentSignatureVerifier);
    }

    // This is a content-signature field from an autograph response.
    const { signature: { x5u, signature } } = metadata;
    const certChain = await (await fetch(x5u)).text();
    // Merge remote records with local ones and serialize as canonical JSON.
    const serialized = await RemoteSettingsWorker.canonicalStringify(localRecords,
                                                                     remoteRecords,
                                                                     timestamp);
    if (!await this._verifier.asyncVerifyContentSignature(serialized,
                                                          "p384ecdsa=" + signature,
                                                          certChain,
                                                          this.signerName)) {
      throw new RemoteSettingsClient.InvalidSignatureError(this.identifier);
    }
  }

  /**
   * Fetch the whole list of records from the server, verify the signature again
   * and then compute a synchronization result as if the diff-based sync happened.
   * And eventually, wipe out the local data.
   *
   * @param {Collection} kintoCollection    Kinto.js Collection instance.
   * @param {int}        expectedTimestamp  Cache busting of collection metadata
   *
   * @returns {Promise<Object>} the computed sync result.
   */
  async _retrySyncFromScratch(kintoCollection, expectedTimestamp) {
    // Fetch collection metadata.
    const api = new KintoHttpClient(gServerURL);
    const client = await api.bucket(this.bucketName).collection(this.collectionName);
    const metadata = await client.getData({ query: { _expected: expectedTimestamp }});
    // Fetch whole list of records.
    const {
      data: remoteRecords,
      last_modified: timestamp,
    } = await client.listRecords({ sort: "id", filters: { _expected: expectedTimestamp } });
    // Verify signature of remote content, before importing it locally.
    await this._validateCollectionSignature(remoteRecords,
                                            timestamp,
                                            metadata);

    // The signature of this remote content is good (we haven't thrown).
    // Now we will store it locally. In order to replicate what `.sync()` returns
    // we will inspect what we had locally.
    const { data: oldData } = await kintoCollection.list({ order: "" }); // no need to sort.

    // We build a sync result as if a diff-based sync was performed.
    const syncResult = { created: [], updated: [], deleted: [] };

    // If the remote last_modified is newer than the local last_modified,
    // replace the local data
    const localLastModified = await kintoCollection.db.getLastModified();
    if (timestamp >= localLastModified) {
      await kintoCollection.clear();
      await kintoCollection.loadDump(remoteRecords);
      await kintoCollection.db.saveLastModified(timestamp);
      await kintoCollection.db.saveMetadata(metadata);

      // Compare local and remote to populate the sync result
      const oldById = new Map(oldData.map(e => [e.id, e]));
      for (const r of remoteRecords) {
        const old = oldById.get(r.id);
        if (old) {
          if (old.last_modified != r.last_modified) {
            syncResult.updated.push({ old, new: r });
          }
          oldById.delete(r.id);
        } else {
          syncResult.created.push(r);
        }
      }
      // Records that remain in our map now are those missing from remote
      syncResult.deleted = Array.from(oldById.values());
    }
    return syncResult;
  }

  /**
   * Use the filter func to filter the lists of changes obtained from synchronization,
   * and return them along with the filtered list of local records.
   *
   * If the filtered lists of changes are all empty, we return null (and thus don't
   * bother listing local DB).
   *
   * @param {Collection} kintoCollection  Kinto.js Collection instance.
   * @param {Object}     syncResult       Synchronization result without filtering.
   *
   * @returns {Promise<Object>} the filtered list of local records, plus the filtered
   *                            list of created, updated and deleted records.
   */
  async _filterSyncResult(kintoCollection, syncResult) {
    // Handle the obtained records (ie. apply locally through events).
    // Build the event data list. It should be filtered (ie. by application target)
    const { created: allCreated, updated: allUpdated, deleted: allDeleted } = syncResult;
    const [created, deleted, updatedFiltered] = await Promise.all(
      [allCreated, allDeleted, allUpdated.map(e => e.new)].map(this._filterEntries.bind(this))
    );
    // For updates, keep entries whose updated form matches the target.
    const updatedFilteredIds = new Set(updatedFiltered.map(e => e.id));
    const updated = allUpdated.filter(({ new: { id } }) => updatedFilteredIds.has(id));

    if (!created.length && !updated.length && !deleted.length) {
      return null;
    }
    // Read local collection of records (also filtered).
    const { data: allData } = await kintoCollection.list({ order: "" }); // no need to sort.
    const current = await this._filterEntries(allData);
    return { created, updated, deleted, current };
  }

  /**
   * Filter entries for which calls to `this.filterFunc` returns null.
   *
   * @param {Array<Objet>} data
   * @returns {Array<Object>}
   */
  async _filterEntries(data) {
    if (!this.filterFunc) {
      return data;
    }
    const environment = cacheProxy(ClientEnvironment);
    const dataPromises = data.map(e => this.filterFunc(e, environment));
    const results = await Promise.all(dataPromises);
    return results.filter(Boolean);
  }
}
