/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemoteSettings"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});
Cu.importGlobalProperties(["fetch"]);

ChromeUtils.defineModuleGetter(this, "Kinto",
                               "resource://services-common/kinto-offline-client.js");
ChromeUtils.defineModuleGetter(this, "KintoHttpClient",
                               "resource://services-common/kinto-http-client.js");
ChromeUtils.defineModuleGetter(this, "CanonicalJSON",
                               "resource://gre/modules/CanonicalJSON.jsm");
ChromeUtils.defineModuleGetter(this, "UptakeTelemetry",
                               "resource://services-common/uptake-telemetry.js");

const PREF_SETTINGS_SERVER             = "services.settings.server";
const PREF_SETTINGS_DEFAULT_BUCKET     = "services.settings.default_bucket";
const PREF_SETTINGS_DEFAULT_SIGNER     = "services.settings.default_signer";
const PREF_SETTINGS_VERIFY_SIGNATURE   = "services.settings.verify_signature";
const PREF_SETTINGS_SERVER_BACKOFF     = "services.settings.server.backoff";
const PREF_SETTINGS_CHANGES_PATH       = "services.settings.changes.path";
const PREF_SETTINGS_LAST_UPDATE        = "services.settings.last_update_seconds";
const PREF_SETTINGS_LAST_ETAG          = "services.settings.last_etag";
const PREF_SETTINGS_CLOCK_SKEW_SECONDS = "services.settings.clock_skew_seconds";
const PREF_SETTINGS_LOAD_DUMP          = "services.settings.load_dump";

// Telemetry update source identifier.
const TELEMETRY_HISTOGRAM_KEY = "settings-changes-monitoring";

const INVALID_SIGNATURE = "Invalid content/signature";


function mergeChanges(collection, localRecords, changes) {
  const records = {};
  // Local records by id.
  localRecords.forEach((record) => records[record.id] = collection.cleanLocalFields(record));
  // All existing records are replaced by the version from the server.
  changes.forEach((record) => records[record.id] = record);

  return Object.values(records)
    // Filter out deleted records.
    .filter((record) => !record.deleted)
    // Sort list by record id.
    .sort((a, b) => {
      if (a.id < b.id) {
        return -1;
      }
      return a.id > b.id ? 1 : 0;
    });
}


function fetchCollectionMetadata(remote, collection) {
  const client = new KintoHttpClient(remote);
  return client.bucket(collection.bucket).collection(collection.name).getData()
    .then(result => {
      return result.signature;
    });
}

function fetchRemoteCollection(remote, collection) {
  const client = new KintoHttpClient(remote);
  return client.bucket(collection.bucket)
           .collection(collection.name)
           .listRecords({sort: "id"});
}

async function fetchLatestChanges(url, lastEtag) {
  //
  // Fetch the list of changes objects from the server that looks like:
  // {"data":[
  //   {
  //     "host":"kinto-ota.dev.mozaws.net",
  //     "last_modified":1450717104423,
  //     "bucket":"blocklists",
  //     "collection":"certificates"
  //    }]}

  // Use ETag to obtain a `304 Not modified` when no change occurred.
  const headers = {};
  if (lastEtag) {
    headers["If-None-Match"] = lastEtag;
  }
  const response = await fetch(url, {headers});

  let changes = [];
  // If no changes since last time, go on with empty list of changes.
  if (response.status != 304) {
    let payload;
    try {
      payload = await response.json();
    } catch (e) {}
    if (!payload.hasOwnProperty("data")) {
      // If the server is failing, the JSON response might not contain the
      // expected data (e.g. error response - Bug 1259145)
      throw new Error(`Server error response ${JSON.stringify(payload)}`);
    }
    changes = payload.data;
  }
  // The server should always return ETag. But we've had situations where the CDN
  // was interfering.
  const currentEtag = response.headers.has("ETag") ? response.headers.get("ETag") : undefined;
  const serverTimeMillis = Date.parse(response.headers.get("Date"));

  // Check if the server asked the clients to back off.
  let backoffSeconds;
  if (response.headers.has("Backoff")) {
    const value = parseInt(response.headers.get("Backoff"), 10);
    if (!isNaN(value)) {
      backoffSeconds = value;
    }
  }

  return {changes, currentEtag, serverTimeMillis, backoffSeconds};
}


class RemoteSettingsClient {

  constructor(collectionName, { lastCheckTimePref, bucketName, signerName }) {
    this.collectionName = collectionName;
    this.lastCheckTimePref = lastCheckTimePref;
    this.bucketName = bucketName;
    this.signerName = signerName;

    this._callbacks = new Map();
    this._callbacks.set("change", []);

    this._kinto = null;
  }

  get identifier() {
    return `${this.bucketName}/${this.collectionName}`;
  }

  get filename() {
    // Replace slash by OS specific path separator (eg. Windows)
    const identifier = OS.Path.join(...this.identifier.split("/"));
    return `${identifier}.json`;
  }

  on(event, callback) {
    if (!this._callbacks.has(event)) {
      throw new Error(`Unknown event type ${event}`);
    }
    this._callbacks.get(event).push(callback);
  }

  /**
   * Open the underlying Kinto collection, using the appropriate adapter and
   * options. This acts as a context manager where the connection is closed
   * once the specified `callback` has finished.
   *
   * @param {callback} function           the async function to execute with the open SQlite connection.
   * @param {Object}   options            additional advanced options.
   * @param {string}   options.hooks      hooks to execute on synchronization (see Kinto.js docs)
   */
  async openCollection(options = {}) {
    if (!this._kinto) {
      this._kinto = new Kinto({ bucket: this.bucketName, adapter: Kinto.adapters.IDB });
    }
    return this._kinto.collection(this.collectionName, options);
  }

  /**
   * Lists settings.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.filters Filter the results (default: `{}`).
   * @param  {Object} options.order   The order to apply   (default: `-last_modified`).
   * @return {Promise}
   */
  async get(options = {}) {
    // In Bug 1451031, we will do some jexl filtering to limit the list items
    // whose target is matched.
    const { filters = {}, order } = options;
    const c = await this.openCollection();
    const { data } = await c.list({ filters, order });
    return data;
  }

  /**
   * Synchronize from Kinto server, if necessary.
   *
   * @param {int}  lastModified       the lastModified date (on the server) for
                                      the remote collection.
   * @param {Date}   serverTime       the current date return by the server.
   * @param {Object} options          additional advanced options.
   * @param {bool}   options.loadDump load initial dump from disk on first sync (default: true)
   * @return {Promise}                which rejects on sync or process failure.
   */
  async maybeSync(lastModified, serverTime, options = { loadDump: true }) {
    const {loadDump} = options;
    const remote = Services.prefs.getCharPref(PREF_SETTINGS_SERVER);
    const verifySignature = Services.prefs.getBoolPref(PREF_SETTINGS_VERIFY_SIGNATURE, true);

    // if there is a signerName and collection signing is enforced, add a
    // hook for incoming changes that validates the signature
    const colOptions = {};
    if (this.signerName && verifySignature) {
      colOptions.hooks = {
        "incoming-changes": [(payload, collection) => {
          return this._validateCollectionSignature(remote, payload, collection);
        }]
      };
    }

    let reportStatus = null;
    try {
      const collection = await this.openCollection(colOptions);
      // Synchronize remote data into a local Sqlite DB.
      let collectionLastModified = await collection.db.getLastModified();

      // If there is no data currently in the collection, attempt to import
      // initial data from the application defaults.
      // This allows to avoid synchronizing the whole collection content on
      // cold start.
      if (!collectionLastModified && loadDump) {
        try {
          const initialData = await this._loadDumpFile();
          await collection.loadDump(initialData.data);
          collectionLastModified = await collection.db.getLastModified();
        } catch (e) {
          // Report but go-on.
          Cu.reportError(e);
        }
      }

      // If the data is up to date, there's no need to sync. We still need
      // to record the fact that a check happened.
      if (lastModified <= collectionLastModified) {
        this._updateLastCheck(serverTime);
        reportStatus = UptakeTelemetry.STATUS.UP_TO_DATE;
        return;
      }

      // Fetch changes from server.
      try {
        // Server changes have priority during synchronization.
        const strategy = Kinto.syncStrategy.SERVER_WINS;
        const { ok } = await collection.sync({remote, strategy});
        if (!ok) {
          // Some synchronization conflicts occured.
          reportStatus = UptakeTelemetry.STATUS.CONFLICT_ERROR;
          throw new Error("Sync failed");
        }
      } catch (e) {
        if (e.message == INVALID_SIGNATURE) {
          // Signature verification failed during synchronzation.
          reportStatus = UptakeTelemetry.STATUS.SIGNATURE_ERROR;
          // if sync fails with a signature error, it's likely that our
          // local data has been modified in some way.
          // We will attempt to fix this by retrieving the whole
          // remote collection.
          const payload = await fetchRemoteCollection(remote, collection);
          try {
            await this._validateCollectionSignature(remote, payload, collection, {ignoreLocal: true});
          } catch (e) {
            reportStatus = UptakeTelemetry.STATUS.SIGNATURE_RETRY_ERROR;
            throw e;
          }
          // if the signature is good (we haven't thrown), and the remote
          // last_modified is newer than the local last_modified, replace the
          // local data
          const localLastModified = await collection.db.getLastModified();
          if (payload.last_modified >= localLastModified) {
            await collection.clear();
            await collection.loadDump(payload.data);
          }
        } else {
          // The sync has thrown, it can be a network or a general error.
          if (/NetworkError/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.NETWORK_ERROR;
          } else if (/Backoff/.test(e.message)) {
            reportStatus = UptakeTelemetry.STATUS.BACKOFF;
          } else {
            reportStatus = UptakeTelemetry.STATUS.SYNC_ERROR;
          }
          throw e;
        }
      }
      // Read local collection of records.
      const { data } = await collection.list();

      // Handle the obtained records (ie. apply locally).
      try {
        // Execute callbacks in order and sequentially.
        // If one fails everything fails.
        const callbacks = this._callbacks.get("change");
        for (const cb of callbacks) {
          await cb({ data });
        }
      } catch (e) {
        reportStatus = UptakeTelemetry.STATUS.APPLY_ERROR;
        throw e;
      }

      // Track last update.
      this._updateLastCheck(serverTime);

    } catch (e) {
      // No specific error was tracked, mark it as unknown.
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      throw e;
    } finally {
      // No error was reported, this is a success!
      if (reportStatus === null) {
        reportStatus = UptakeTelemetry.STATUS.SUCCESS;
      }
      // Report success/error status to Telemetry.
      UptakeTelemetry.report(this.identifier, reportStatus);
    }
  }

  /**
   * Load the the JSON file distributed with the release for this collection.
   */
  async _loadDumpFile() {
    // Replace OS specific path separator by / for URI.
    const { components: folderFile } = OS.Path.split(this.filename);
    const fileURI = `resource://app/defaults/${folderFile.join("/")}`;
    const response = await fetch(fileURI);
    if (!response.ok) {
      throw new Error(`Could not read from '${fileURI}'`);
    }
    // Will be rejected if JSON is invalid.
    return response.json();
  }

  async _validateCollectionSignature(remote, payload, collection, options = {}) {
    const {ignoreLocal} = options;

    // this is a content-signature field from an autograph response.
    const {x5u, signature} = await fetchCollectionMetadata(remote, collection);
    const certChainResponse = await fetch(x5u);
    const certChain = await certChainResponse.text();

    const verifier = Cc["@mozilla.org/security/contentsignatureverifier;1"]
                       .createInstance(Ci.nsIContentSignatureVerifier);

    let toSerialize;
    if (ignoreLocal) {
      toSerialize = {
        last_modified: `${payload.last_modified}`,
        data: payload.data
      };
    } else {
      const {data: localRecords} = await collection.list();
      const records = mergeChanges(collection, localRecords, payload.changes);
      toSerialize = {
        last_modified: `${payload.lastModified}`,
        data: records
      };
    }

    const serialized = CanonicalJSON.stringify(toSerialize);

    if (verifier.verifyContentSignature(serialized, "p384ecdsa=" + signature,
                                        certChain,
                                        this.signerName)) {
      // In case the hash is valid, apply the changes locally.
      return payload;
    }
    throw new Error(INVALID_SIGNATURE);
  }

  /**
   * Save last time server was checked in users prefs.
   *
   * @param {Date} serverTime   the current date return by server.
   */
  _updateLastCheck(serverTime) {
    if (!this.lastCheckTimePref) {
      // If not set (default), it is not necessary to store the last check timestamp.
      return;
    }
    // Storing the last check time is mainly a matter of retro-compatibility with
    // the blocklists clients.
    const checkedServerTimeInSeconds = Math.round(serverTime / 1000);
    Services.prefs.setIntPref(this.lastCheckTimePref, checkedServerTimeInSeconds);
  }
}


function remoteSettingsFunction() {
  const _clients = new Map();

  // If not explicitly specified, use the default bucket name and signer.
  const mainBucket = Services.prefs.getCharPref(PREF_SETTINGS_DEFAULT_BUCKET);
  const defaultSigner = Services.prefs.getCharPref(PREF_SETTINGS_DEFAULT_SIGNER);

  const remoteSettings = function(collectionName, options) {
    // Get or instantiate a remote settings client.
    const rsOptions = {
      bucketName: mainBucket,
      signerName: defaultSigner,
      ...options
    };
    const { bucketName } = rsOptions;
    const key = `${bucketName}/${collectionName}`;
    if (!_clients.has(key)) {
      const c = new RemoteSettingsClient(collectionName, rsOptions);
      _clients.set(key, c);
    }
    return _clients.get(key);
  };

  // This is called by the ping mechanism.
  // returns a promise that rejects if something goes wrong
  remoteSettings.pollChanges = async () => {
    // Check if the server backoff time is elapsed.
    if (Services.prefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF)) {
      const backoffReleaseTime = Services.prefs.getCharPref(PREF_SETTINGS_SERVER_BACKOFF);
      const remainingMilliseconds = parseInt(backoffReleaseTime, 10) - Date.now();
      if (remainingMilliseconds > 0) {
        // Backoff time has not elapsed yet.
        UptakeTelemetry.report(TELEMETRY_HISTOGRAM_KEY,
                               UptakeTelemetry.STATUS.BACKOFF);
        throw new Error(`Server is asking clients to back off; retry in ${Math.ceil(remainingMilliseconds / 1000)}s.`);
      } else {
        Services.prefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      }
    }

    // Right now, we only use the collection name and the last modified info
    const kintoBase = Services.prefs.getCharPref(PREF_SETTINGS_SERVER);
    const changesEndpoint = kintoBase + Services.prefs.getCharPref(PREF_SETTINGS_CHANGES_PATH);

    let lastEtag;
    if (Services.prefs.prefHasUserValue(PREF_SETTINGS_LAST_ETAG)) {
      lastEtag = Services.prefs.getCharPref(PREF_SETTINGS_LAST_ETAG);
    }

    let pollResult;
    try {
      pollResult = await fetchLatestChanges(changesEndpoint, lastEtag);
    } catch (e) {
      // Report polling error to Uptake Telemetry.
      let report;
      if (/Server/.test(e.message)) {
        report = UptakeTelemetry.STATUS.SERVER_ERROR;
      } else if (/NetworkError/.test(e.message)) {
        report = UptakeTelemetry.STATUS.NETWORK_ERROR;
      } else {
        report = UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      UptakeTelemetry.report(TELEMETRY_HISTOGRAM_KEY, report);
      // No need to go further.
      throw new Error(`Polling for changes failed: ${e.message}.`);
    }

    const {serverTimeMillis, changes, currentEtag, backoffSeconds} = pollResult;

    // Report polling success to Uptake Telemetry.
    const report = changes.length == 0 ? UptakeTelemetry.STATUS.UP_TO_DATE
                                       : UptakeTelemetry.STATUS.SUCCESS;
    UptakeTelemetry.report(TELEMETRY_HISTOGRAM_KEY, report);

    // Check if the server asked the clients to back off (for next poll).
    if (backoffSeconds) {
      const backoffReleaseTime = Date.now() + backoffSeconds * 1000;
      Services.prefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, backoffReleaseTime);
    }

    // Record new update time and the difference between local and server time.
    // Negative clockDifference means local time is behind server time
    // by the absolute of that value in seconds (positive means it's ahead)
    const clockDifference = Math.floor((Date.now() - serverTimeMillis) / 1000);
    Services.prefs.setIntPref(PREF_SETTINGS_CLOCK_SKEW_SECONDS, clockDifference);
    Services.prefs.setIntPref(PREF_SETTINGS_LAST_UPDATE, serverTimeMillis / 1000);

    const loadDump = Services.prefs.getBoolPref(PREF_SETTINGS_LOAD_DUMP, true);
    // Iterate through the collections version info and initiate a synchronization
    // on the related remote settings client.
    let firstError;
    for (const change of changes) {
      const {bucket, collection, last_modified: lastModified} = change;
      const key = `${bucket}/${collection}`;
      if (!_clients.has(key)) {
        continue;
      }
      const client = _clients.get(key);
      if (client.bucketName != bucket) {
        continue;
      }
      try {
        await client.maybeSync(lastModified, serverTimeMillis, {loadDump});
      } catch (e) {
        if (!firstError) {
          firstError = e;
        }
      }
    }
    if (firstError) {
      // cause the promise to reject by throwing the first observed error
      throw firstError;
    }

    // Save current Etag for next poll.
    if (currentEtag) {
      Services.prefs.setCharPref(PREF_SETTINGS_LAST_ETAG, currentEtag);
    }

    Services.obs.notifyObservers(null, "remote-settings-changes-polled");
  };

  return remoteSettings;
}

var RemoteSettings = remoteSettingsFunction();
