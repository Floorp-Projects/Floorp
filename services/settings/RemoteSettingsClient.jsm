/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemoteSettingsClient"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  ClientEnvironmentBase:
    "resource://gre/modules/components-utils/ClientEnvironment.jsm",
  Database: "resource://services-settings/Database.jsm",
  Downloader: "resource://services-settings/Attachments.jsm",
  IDBHelpers: "resource://services-settings/IDBHelpers.jsm",
  KintoHttpClient: "resource://services-common/kinto-http-client.js",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  PerformanceCounters: "resource://gre/modules/PerformanceCounters.jsm",
  RemoteSettingsWorker: "resource://services-settings/RemoteSettingsWorker.jsm",
  UptakeTelemetry: "resource://services-common/uptake-telemetry.js",
  Utils: "resource://services-settings/Utils.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const TELEMETRY_COMPONENT = "remotesettings";

XPCOMUtils.defineLazyGetter(this, "console", () => Utils.log);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gTimingEnabled",
  "services.settings.enablePerformanceCounters",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gLoadDump",
  "services.settings.load_dump",
  true
);

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

class NetworkOfflineError extends Error {
  constructor(cid) {
    super("Network is offline");
    this.name = "NetworkOfflineError";
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

class UnknownCollectionError extends Error {
  constructor(cid) {
    super(`Unknown Collection "${cid}"`);
    this.name = "UnknownCollectionError";
  }
}

class AttachmentDownloader extends Downloader {
  constructor(client) {
    super(client.bucketName, client.collectionName);
    this._client = client;
  }

  get cacheImpl() {
    const cacheImpl = {
      get: async attachmentId => {
        return this._client.db.getAttachment(attachmentId);
      },
      set: async (attachmentId, attachment) => {
        return this._client.db.saveAttachment(attachmentId, attachment);
      },
      delete: async attachmentId => {
        return this._client.db.saveAttachment(attachmentId, null);
      },
    };
    Object.defineProperty(this, "cacheImpl", { value: cacheImpl });
    return cacheImpl;
  }

  /**
   * Download attachment and report Telemetry on failure.
   *
   * @see Downloader.download
   */
  async download(record, options) {
    try {
      // Explicitly await here to ensure we catch a network error.
      return await super.download(record, options);
    } catch (err) {
      // Report download error.
      let status = UptakeTelemetry.STATUS.DOWNLOAD_ERROR;
      if (Utils.isOffline) {
        status = UptakeTelemetry.STATUS.NETWORK_OFFLINE_ERROR;
      } else if (/NetworkError/.test(err.message)) {
        status = UptakeTelemetry.STATUS.NETWORK_ERROR;
      }
      // If the file failed to be downloaded, report it as such in Telemetry.
      await UptakeTelemetry.report(TELEMETRY_COMPONENT, status, {
        source: this._client.identifier,
      });
      throw err;
    }
  }

  /**
   * Delete all downloaded records attachments.
   *
   * Note: the list of attachments to be deleted is based on the
   * current list of records.
   */
  async deleteAll() {
    let allRecords = await this._client.db.list();
    return Promise.all(
      allRecords.filter(r => !!r.attachment).map(r => this.delete(r))
    );
  }
}

class RemoteSettingsClient extends EventEmitter {
  static get NetworkOfflineError() {
    return NetworkOfflineError;
  }
  static get InvalidSignatureError() {
    return InvalidSignatureError;
  }
  static get MissingSignatureError() {
    return MissingSignatureError;
  }
  static get UnknownCollectionError() {
    return UnknownCollectionError;
  }

  constructor(
    collectionName,
    {
      bucketNamePref,
      signerName,
      filterFunc,
      localFields = [],
      lastCheckTimePref,
    }
  ) {
    super(["sync"]); // emitted events

    this.collectionName = collectionName;
    this.signerName = signerName;
    this.filterFunc = filterFunc;
    this.localFields = localFields;
    this._lastCheckTimePref = lastCheckTimePref;
    this._verifier = null;
    this._syncRunning = false;

    // This attribute allows signature verification to be disabled, when running tests
    // or when pulling data from a dev server.
    this.verifySignature = true;

    // The bucket preference value can be changed (eg. `main` to `main-preview`) in order
    // to preview the changes to be approved in a real client.
    this.bucketNamePref = bucketNamePref;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "bucketName",
      this.bucketNamePref
    );

    XPCOMUtils.defineLazyGetter(
      this,
      "db",
      () => new Database(this.identifier)
    );

    XPCOMUtils.defineLazyGetter(
      this,
      "attachments",
      () => new AttachmentDownloader(this)
    );
  }

  get identifier() {
    return `${this.bucketName}/${this.collectionName}`;
  }

  get lastCheckTimePref() {
    return (
      this._lastCheckTimePref ||
      `services.settings.${this.bucketName}.${this.collectionName}.last_check`
    );
  }

  httpClient() {
    const api = new KintoHttpClient(Utils.SERVER_URL);
    return api.bucket(this.bucketName).collection(this.collectionName);
  }

  /**
   * Retrieve the collection timestamp for the last synchronization.
   *
   * @returns {number}
   *          The timestamp in milliseconds, returns -1 if retrieving
   *          the timestamp from the kinto collection fails.
   */
  async getLastModified() {
    let timestamp = -1;
    try {
      timestamp = await this.db.getLastModified();
    } catch (err) {
      console.warn(
        `Error retrieving the getLastModified timestamp from ${this.identifier} RemoteSettingsClient`,
        err
      );
    }

    return timestamp;
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
        if (
          gLoadDump &&
          (await Utils.hasLocalDump(this.bucketName, this.collectionName))
        ) {
          // Since there is a JSON dump, load it as default data.
          console.debug(`${this.identifier} Local DB is empty, load JSON dump`);
          await this._importJSONDump();
        } else {
          // There is no JSON dump, force a synchronization from the server.
          console.debug(
            `${this.identifier} Local DB is empty, pull data from server`
          );
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
    const data = await this.db.list({ filters, order });
    console.debug(
      `${this.identifier} ${data.length} records before filtering.`
    );

    if (verifySignature) {
      console.debug(
        `${this.identifier} verify signature of local data on read`
      );
      const allData = ObjectUtils.isEmpty(filters)
        ? data
        : await this.db.list();
      const localRecords = allData.map(r => this._cleanLocalFields(r));
      const timestamp = await this.db.getLastModified();
      let metadata = await this.db.getMetadata();
      if (syncIfEmpty && ObjectUtils.isEmpty(metadata)) {
        // No sync occured yet, may have records from dump but no metadata.
        console.debug(
          `Required metadata for ${this.identifier}, fetching from server.`
        );
        metadata = await this.httpClient().getData();
        await this.db.saveMetadata(metadata);
      }
      await this._validateCollectionSignature(
        localRecords,
        timestamp,
        metadata
      );
    }

    // Filter the records based on `this.filterFunc` results.
    const final = await this._filterEntries(data);
    console.debug(
      `${this.identifier} ${final.length} records after filtering.`
    );
    return final;
  }

  /**
   * Synchronize the local database with the remote server.
   *
   * @param {Object} options See #maybeSync() options.
   */
  async sync(options) {
    // We want to know which timestamp we are expected to obtain in order to leverage
    // cache busting. We don't provide ETag because we don't want a 304.
    const { changes } = await Utils.fetchLatestChanges(Utils.SERVER_URL, {
      filters: {
        collection: this.collectionName,
        bucket: this.bucketName,
      },
    });
    if (changes.length === 0) {
      throw new RemoteSettingsClient.UnknownCollectionError(this.identifier);
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
   * @param {bool}   options.loadDump  load initial dump from disk on first sync (default: true, unless
   *                                   `services.settings.load_dump` says otherwise).
   * @param {string} options.trigger   label to identify what triggered this sync (eg. ``"timer"``, default: `"manual"`)
   * @return {Promise}                 which rejects on sync or process failure.
   */
  async maybeSync(expectedTimestamp, options = {}) {
    // Should the clients try to load JSON dump? (mainly disabled in tests)
    const { loadDump = gLoadDump, trigger = "manual" } = options;

    // Make sure we don't run several synchronizations in parallel, mainly
    // in order to avoid race conditions in "sync" events listeners.
    if (this._syncRunning) {
      console.warn(`${this.identifier} sync already running`);
      return;
    }

    // Prevent network requests and IndexedDB calls to be initiated
    // during shutdown.
    if (Services.startup.shuttingDown) {
      console.warn(`${this.identifier} sync interrupted by shutdown`);
      return;
    }

    this._syncRunning = true;

    let importedFromDump = [];
    const startedAt = new Date();
    let reportStatus = null;
    let thrownError = null;
    try {
      // If network is offline, we can't synchronize.
      if (Utils.isOffline) {
        throw new RemoteSettingsClient.NetworkOfflineError();
      }

      // Read last timestamp and local data before sync.
      let collectionLastModified = await this.db.getLastModified();
      const allData = await this.db.list();
      // Local data can contain local fields, strip them.
      let localRecords = allData.map(r => this._cleanLocalFields(r));
      const localMetadata = await this.db.getMetadata();

      // If there is no data currently in the collection, attempt to import
      // initial data from the application defaults.
      // This allows to avoid synchronizing the whole collection content on
      // cold start.
      if (!collectionLastModified && loadDump) {
        try {
          const imported = await this._importJSONDump();
          // The worker only returns an integer. List the imported records to build the sync event.
          if (imported > 0) {
            console.debug(
              `${this.identifier} ${imported} records loaded from JSON dump`
            );
            importedFromDump = await this.db.list();
            // Local data is the data loaded from dump. We will need this later
            // to compute the sync result.
            localRecords = importedFromDump;
          }
          collectionLastModified = await this.db.getLastModified();
        } catch (e) {
          // Report but go-on.
          Cu.reportError(e);
        }
      }

      let syncResult;
      try {
        // Is local timestamp up to date with the server?
        if (expectedTimestamp <= collectionLastModified) {
          console.debug(`${this.identifier} local data is up-to-date`);
          reportStatus = UptakeTelemetry.STATUS.UP_TO_DATE;

          // If the data is up-to-date but don't have metadata (records loaded from dump),
          // we fetch them and validate the signature immediately.
          if (this.verifySignature && ObjectUtils.isEmpty(localMetadata)) {
            console.debug(`${this.identifier} pull collection metadata`);
            const metadata = await this.httpClient().getData({
              query: { _expected: expectedTimestamp },
            });
            await this.db.saveMetadata(metadata);
            // We don't bother validating the signature if the dump was just loaded. We do
            // if the dump was loaded at some other point (eg. from .get()).
            if (this.verifySignature && importedFromDump.length == 0) {
              console.debug(
                `${this.identifier} verify signature of local data`
              );
              await this._validateCollectionSignature(
                localRecords,
                collectionLastModified,
                metadata
              );
            }
          }

          // Since the data is up-to-date, if we didn't load any dump then we're done here.
          if (importedFromDump.length == 0) {
            return;
          }
          // Otherwise we want to continue with sending the sync event to notify about the created records.
          syncResult = {
            current: importedFromDump,
            created: importedFromDump,
            updated: [],
            deleted: [],
          };
        } else {
          // Local data is outdated.
          // Fetch changes from server, and make sure we overwrite local data.
          const startSyncDB = Cu.now() * 1000;
          syncResult = await this._importChanges(
            localRecords,
            collectionLastModified,
            localMetadata,
            expectedTimestamp
          );
          if (gTimingEnabled) {
            const endSyncDB = Cu.now() * 1000;
            PerformanceCounters.storeExecutionTime(
              `remotesettings/${this.identifier}`,
              "syncDB",
              endSyncDB - startSyncDB,
              "duration"
            );
          }
          // The records imported from the dump should be considered as "created" for the
          // listeners.
          const importedById = importedFromDump.reduce((acc, r) => {
            acc.set(r.id, r);
            return acc;
          }, new Map());
          // Deleted records should not appear as created.
          syncResult.deleted.forEach(r => importedById.delete(r.id));
          // Records from dump that were updated should appear in their newest form.
          syncResult.updated.forEach(u => {
            if (importedById.has(u.old.id)) {
              importedById.set(u.old.id, u.new);
            }
          });
          syncResult.created = syncResult.created.concat(
            Array.from(importedById.values())
          );
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
            console.warn(
              `Signature verified failed for ${this.identifier}. Retry from scratch`
            );
            syncResult = await this._importChanges(
              localRecords,
              collectionLastModified,
              localMetadata,
              expectedTimestamp,
              { retry: true }
            );
          } catch (e) {
            // If the signature fails again, or if an error occured during wiping out the
            // local data, then we report it as a *signature retry* error.
            reportStatus = UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR;
            throw e;
          }
        } else {
          // The sync has thrown for other reason than signature verification.
          // Default status for errors at this step is SYNC_ERROR.
          reportStatus = this._telemetryFromError(e, {
            default: UptakeTelemetry.STATUS.SYNC_ERROR,
          });
          throw e;
        }
      }
      // Filter the synchronization results using `filterFunc` (ie. JEXL).
      const filteredSyncResult = await this._filterSyncResult(syncResult);
      // If every changed entry is filtered, we don't even fire the event.
      if (filteredSyncResult) {
        try {
          await this.emit("sync", { data: filteredSyncResult });
        } catch (e) {
          reportStatus = UptakeTelemetry.STATUS.APPLY_ERROR;
          throw e;
        }
      } else {
        console.info(
          `All changes are filtered by JEXL expressions for ${this.identifier}`
        );
      }
    } catch (e) {
      thrownError = e;
      // If browser is shutting down, then we can report a specific status.
      // (eg. IndexedDB will abort transactions)
      if (Services.startup.shuttingDown) {
        reportStatus = UptakeTelemetry.STATUS.SHUTDOWN_ERROR;
      }
      // If no Telemetry status was determined yet (ie. outside sync step),
      // then introspect error, default status at this step is UNKNOWN.
      else if (reportStatus == null) {
        reportStatus = this._telemetryFromError(e, {
          default: UptakeTelemetry.STATUS.UNKNOWN_ERROR,
        });
      }
      throw e;
    } finally {
      const durationMilliseconds = new Date() - startedAt;
      // No error was reported, this is a success!
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.SUCCESS;
      }
      // Report success/error status to Telemetry.
      let reportArgs = {
        source: this.identifier,
        trigger,
        duration: durationMilliseconds,
      };
      // In Bug 1617133, we will try to break down specific errors into
      // more precise statuses by reporting the JavaScript error name
      // ("TypeError", etc.) to Telemetry on Nightly.
      const channel = UptakeTelemetry.Policy.getChannel();
      if (
        thrownError !== null &&
        channel == "nightly" &&
        [
          UptakeTelemetry.STATUS.SYNC_ERROR,
          UptakeTelemetry.STATUS.CUSTOM_1_ERROR, // IndexedDB.
          UptakeTelemetry.STATUS.UNKNOWN_ERROR,
          UptakeTelemetry.STATUS.SHUTDOWN_ERROR,
        ].includes(reportStatus)
      ) {
        // List of possible error names for IndexedDB:
        // https://searchfox.org/mozilla-central/rev/49ed791/dom/base/DOMException.cpp#28-53
        reportArgs = { ...reportArgs, errorName: thrownError.name };
      }

      await UptakeTelemetry.report(
        TELEMETRY_COMPONENT,
        reportStatus,
        reportArgs
      );

      console.debug(`${this.identifier} sync status is ${reportStatus}`);
      this._syncRunning = false;
    }
  }

  /**
   * Determine the Telemetry uptake status based on the specified
   * error.
   */
  _telemetryFromError(e, options = { default: null }) {
    let reportStatus = options.default;

    if (e instanceof RemoteSettingsClient.NetworkOfflineError) {
      reportStatus = UptakeTelemetry.STATUS.NETWORK_OFFLINE_ERROR;
    } else if (e instanceof RemoteSettingsClient.MissingSignatureError) {
      // Collection metadata has no signature info, no need to retry.
      reportStatus = UptakeTelemetry.STATUS.SIGNATURE_ERROR;
    } else if (e instanceof IDBHelpers.ShutdownError) {
      reportStatus = UptakeTelemetry.STATUS.SHUTDOWN_ERROR;
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
    } else if (
      // Errors from kinto.js IDB adapter.
      e instanceof IDBHelpers.IndexedDBError ||
      // Other IndexedDB errors (eg. RemoteSettingsWorker).
      /IndexedDB/.test(e.message)
    ) {
      reportStatus = UptakeTelemetry.STATUS.CUSTOM_1_ERROR;
    }

    return reportStatus;
  }

  /**
   * Import the JSON files from services/settings/dump into the local DB.
   */
  async _importJSONDump() {
    const start = Cu.now() * 1000;
    const result = await RemoteSettingsWorker.importJSONDump(
      this.bucketName,
      this.collectionName
    );
    if (gTimingEnabled) {
      const end = Cu.now() * 1000;
      PerformanceCounters.storeExecutionTime(
        `remotesettings/${this.identifier}`,
        "importJSONDump",
        end - start,
        "duration"
      );
    }
    return result;
  }

  /**
   * Fetch the signature info from the collection metadata and verifies that the
   * local set of records has the same.
   *
   * @param {Array<Object>} records The list of records to validate.
   * @param {int} timestamp         The timestamp associated with the list of remote records.
   * @param {Object} metadata       The collection metadata, that contains the signature payload.
   * @returns {Promise}
   */
  async _validateCollectionSignature(records, timestamp, metadata) {
    const start = Cu.now() * 1000;

    if (!metadata || !metadata.signature) {
      throw new RemoteSettingsClient.MissingSignatureError(this.identifier);
    }

    if (!this._verifier) {
      this._verifier = Cc[
        "@mozilla.org/security/contentsignatureverifier;1"
      ].createInstance(Ci.nsIContentSignatureVerifier);
    }

    // This is a content-signature field from an autograph response.
    const {
      signature: { x5u, signature },
    } = metadata;
    const certChain = await (await fetch(x5u)).text();
    // Merge remote records with local ones and serialize as canonical JSON.
    const serialized = await RemoteSettingsWorker.canonicalStringify(
      records,
      timestamp
    );
    if (
      !(await this._verifier.asyncVerifyContentSignature(
        serialized,
        "p384ecdsa=" + signature,
        certChain,
        this.signerName
      ))
    ) {
      throw new RemoteSettingsClient.InvalidSignatureError(this.identifier);
    }
    if (gTimingEnabled) {
      const end = Cu.now() * 1000;
      PerformanceCounters.storeExecutionTime(
        `remotesettings/${this.identifier}`,
        "validateCollectionSignature",
        end - start,
        "duration"
      );
    }
  }

  /**
   * This method is in charge of fetching data from server, applying the diff-based
   * changes to the local DB, validating the signature, and computing a synchronization
   * result with the list of creation, updates, and deletions.
   *
   * @param {Array<Object>} localRecords      Current list of records in local DB.
   * @param {int}           localTimestamp    Current timestamp in local DB.
   * @param {Object}        localMetadata     Current metadata in local DB.
   * @param {int}           expectedTimestamp Cache busting of collection metadata
   * @param {Object}        options
   * @param {bool}          options.retry     Whether this method is called in the
   *                                          retry situation.
   *
   * @returns {Promise<Object>} the computed sync result.
   */
  async _importChanges(
    localRecords,
    localTimestamp,
    localMetadata,
    expectedTimestamp,
    options = {}
  ) {
    const { retry = false } = options;
    const since = retry || !localTimestamp ? undefined : `"${localTimestamp}"`;

    // Fetch collection metadata and list of changes from server.
    console.debug(
      `${this.identifier} Fetch changes from server (expected=${expectedTimestamp}, since=${since})`
    );
    const {
      metadata,
      remoteTimestamp,
      remoteRecords,
    } = await this._fetchChangeset(expectedTimestamp, since);

    // We build a sync result, based on remote changes.
    const syncResult = {
      current: localRecords,
      created: [],
      updated: [],
      deleted: [],
    };
    // If data wasn't changed, return empty sync result.
    // This can happen when we update the signature but not the data.
    console.debug(
      `${this.identifier} local timestamp: ${localTimestamp}, remote: ${remoteTimestamp}`
    );
    if (localTimestamp && remoteTimestamp < localTimestamp) {
      return syncResult;
    }

    // Separate tombstones from creations/updates.
    const toDelete = remoteRecords.filter(r => r.deleted);
    const toInsert = remoteRecords.filter(r => !r.deleted);
    console.debug(
      `${this.identifier} ${toDelete.length} to delete, ${toInsert.length} to insert`
    );

    const start = Cu.now() * 1000;
    // Delete local records for each tombstone.
    await this.db.deleteBulk(toDelete);
    // Overwrite all other data.
    await this.db.importBulk(toInsert);
    await this.db.saveLastModified(remoteTimestamp);
    await this.db.saveMetadata(metadata);
    if (gTimingEnabled) {
      const end = Cu.now() * 1000;
      PerformanceCounters.storeExecutionTime(
        `remotesettings/${this.identifier}`,
        "loadRawData",
        end - start,
        "duration"
      );
    }

    // Read the new local data, after updating.
    const newLocal = await this.db.list();
    const newRecords = newLocal.map(r => this._cleanLocalFields(r));
    // And verify the signature on what is now stored.
    if (this.verifySignature) {
      try {
        await this._validateCollectionSignature(
          newRecords,
          remoteTimestamp,
          metadata
        );
      } catch (e) {
        // Signature failed, clear local data.
        console.debug(`${this.identifier} clear local data`);
        await this.db.clear();

        // If signature failed again after retry, then
        // we restore the local data that we had before sync.
        if (retry) {
          try {
            // Make sure the local data before sync was not tampered.
            await this._validateCollectionSignature(
              localRecords,
              localTimestamp,
              localMetadata
            );
            // Signature of data before sync is good. Restore it.
            await this.db.importBulk(localRecords);
            await this.db.saveLastModified(localTimestamp);
            await this.db.saveMetadata(localMetadata);
          } catch (_) {
            // Local data before sync was tampered. Restore dump if available.
            if (
              await Utils.hasLocalDump(this.bucketName, this.collectionName)
            ) {
              await this._importJSONDump();
            }
          }
        }
        throw e;
      }
    } else {
      console.warn(`${this.identifier} has signature disabled`);
    }

    // Compute the changes, comparing records before and after.
    syncResult.current = newRecords;
    const oldById = new Map(localRecords.map(e => [e.id, e]));
    for (const r of newRecords) {
      const old = oldById.get(r.id);
      if (old) {
        oldById.delete(r.id);
        if (r.last_modified != old.last_modified) {
          syncResult.updated.push({ old, new: r });
        }
      } else {
        syncResult.created.push(r);
      }
    }
    syncResult.deleted = syncResult.deleted.concat(
      Array.from(oldById.values())
    );
    console.debug(
      `${this.identifier} ${syncResult.created.length} created. ${syncResult.updated.length} updated. ${syncResult.deleted.length} deleted.`
    );

    return syncResult;
  }

  /**
   * Fetch information from changeset endpoint.
   *
   * @param expectedTimestamp cache busting value
   * @param since timestamp of last sync (optional)
   */
  async _fetchChangeset(expectedTimestamp, since) {
    const client = this.httpClient();
    const {
      metadata,
      timestamp: remoteTimestamp,
      changes: remoteRecords,
    } = await client.execute(
      {
        path: `/buckets/${this.bucketName}/collections/${this.collectionName}/changeset`,
      },
      {
        query: {
          _expected: expectedTimestamp,
          _since: since,
        },
      }
    );
    return {
      remoteTimestamp,
      metadata,
      remoteRecords,
    };
  }

  /**
   * Use the filter func to filter the lists of changes obtained from synchronization,
   * and return them along with the filtered list of local records.
   *
   * If the filtered lists of changes are all empty, we return null (and thus don't
   * bother listing local DB).
   *
   * @param {Object}     syncResult       Synchronization result without filtering.
   *
   * @returns {Promise<Object>} the filtered list of local records, plus the filtered
   *                            list of created, updated and deleted records.
   */
  async _filterSyncResult(syncResult) {
    // Handle the obtained records (ie. apply locally through events).
    // Build the event data list. It should be filtered (ie. by application target)
    const {
      current: allData,
      created: allCreated,
      updated: allUpdated,
      deleted: allDeleted,
    } = syncResult;
    const [created, deleted, updatedFiltered] = await Promise.all(
      [allCreated, allDeleted, allUpdated.map(e => e.new)].map(
        this._filterEntries.bind(this)
      )
    );
    // For updates, keep entries whose updated form matches the target.
    const updatedFilteredIds = new Set(updatedFiltered.map(e => e.id));
    const updated = allUpdated.filter(({ new: { id } }) =>
      updatedFilteredIds.has(id)
    );

    if (!created.length && !updated.length && !deleted.length) {
      return null;
    }
    // Read local collection of records (also filtered).
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
    const start = Cu.now() * 1000;
    const environment = cacheProxy(ClientEnvironmentBase);
    const dataPromises = data.map(e => this.filterFunc(e, environment));
    const results = await Promise.all(dataPromises);
    if (gTimingEnabled) {
      const end = Cu.now() * 1000;
      PerformanceCounters.storeExecutionTime(
        `remotesettings/${this.identifier}`,
        "filterEntries",
        end - start,
        "duration"
      );
    }
    return results.filter(Boolean);
  }

  /**
   * Remove the fields from the specified record
   * that are not present on server.
   *
   * @param {Object} record
   */
  _cleanLocalFields(record) {
    const keys = ["_status"].concat(this.localFields);
    const result = { ...record };
    for (const key of keys) {
      delete result[key];
    }
    return result;
  }
}
