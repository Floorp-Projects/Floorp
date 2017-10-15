/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AddonBlocklistClient",
                         "GfxBlocklistClient",
                         "OneCRLBlocklistClient",
                         "PinningBlocklistClient",
                         "PluginBlocklistClient"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});
Cu.importGlobalProperties(["fetch"]);

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Kinto",
                                  "resource://services-common/kinto-offline-client.js");
XPCOMUtils.defineLazyModuleGetter(this, "KintoHttpClient",
                                  "resource://services-common/kinto-http-client.js");
XPCOMUtils.defineLazyModuleGetter(this, "FirefoxAdapter",
                                  "resource://services-common/kinto-storage-adapter.js");
XPCOMUtils.defineLazyModuleGetter(this, "CanonicalJSON",
                                  "resource://gre/modules/CanonicalJSON.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UptakeTelemetry",
                                  "resource://services-common/uptake-telemetry.js");

const KEY_APPDIR                             = "XCurProcD";
const PREF_SETTINGS_SERVER                   = "services.settings.server";
const PREF_BLOCKLIST_BUCKET                  = "services.blocklist.bucket";
const PREF_BLOCKLIST_ONECRL_COLLECTION       = "services.blocklist.onecrl.collection";
const PREF_BLOCKLIST_ONECRL_CHECKED_SECONDS  = "services.blocklist.onecrl.checked";
const PREF_BLOCKLIST_ADDONS_COLLECTION       = "services.blocklist.addons.collection";
const PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS  = "services.blocklist.addons.checked";
const PREF_BLOCKLIST_PLUGINS_COLLECTION      = "services.blocklist.plugins.collection";
const PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS = "services.blocklist.plugins.checked";
const PREF_BLOCKLIST_PINNING_ENABLED         = "services.blocklist.pinning.enabled";
const PREF_BLOCKLIST_PINNING_BUCKET          = "services.blocklist.pinning.bucket";
const PREF_BLOCKLIST_PINNING_COLLECTION      = "services.blocklist.pinning.collection";
const PREF_BLOCKLIST_PINNING_CHECKED_SECONDS = "services.blocklist.pinning.checked";
const PREF_BLOCKLIST_GFX_COLLECTION          = "services.blocklist.gfx.collection";
const PREF_BLOCKLIST_GFX_CHECKED_SECONDS     = "services.blocklist.gfx.checked";
const PREF_BLOCKLIST_ENFORCE_SIGNING         = "services.blocklist.signing.enforced";

const INVALID_SIGNATURE = "Invalid content/signature";

// This was the default path in earlier versions of
// FirefoxAdapter, so for backwards compatibility we maintain this
// filename, even though it isn't descriptive of who is using it.
const KINTO_STORAGE_PATH = "kinto.sqlite";



function mergeChanges(collection, localRecords, changes) {
  const records = {};
  // Local records by id.
  localRecords.forEach((record) => records[record.id] = collection.cleanLocalFields(record));
  // All existing records are replaced by the version from the server.
  changes.forEach((record) => records[record.id] = record);

  return Object.values(records)
    // Filter out deleted records.
    .filter((record) => record.deleted != true)
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


class BlocklistClient {

  constructor(collectionName, lastCheckTimePref, processCallback, bucketName, signerName) {
    this.collectionName = collectionName;
    this.lastCheckTimePref = lastCheckTimePref;
    this.processCallback = processCallback;
    this.bucketName = bucketName;
    this.signerName = signerName;

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

  /**
   * Open the underlying Kinto collection, using the appropriate adapter and
   * options. This acts as a context manager where the connection is closed
   * once the specified `callback` has finished.
   *
   * @param {callback} function           the async function to execute with the open SQlite connection.
   * @param {Object}   options            additional advanced options.
   * @param {string}   options.bucket     override bucket name of client (default: this.bucketName)
   * @param {string}   options.collection override collection name of client (default: this.collectionName)
   * @param {string}   options.path       override default Sqlite path (default: kinto.sqlite)
   * @param {string}   options.hooks      hooks to execute on synchronization (see Kinto.js docs)
   */
  async openCollection(callback, options = {}) {
    const { bucket = this.bucketName, path = KINTO_STORAGE_PATH } = options;
    if (!this._kinto) {
      this._kinto = new Kinto({bucket, adapter: FirefoxAdapter});
    }
    let sqliteHandle;
    try {
      sqliteHandle = await FirefoxAdapter.openConnection({path});
      const colOptions = Object.assign({adapterOptions: {sqliteHandle}}, options);
      const {collection: collectionName = this.collectionName} = options;
      const collection = this._kinto.collection(collectionName, colOptions);
      return await callback(collection);
    } finally {
      if (sqliteHandle) {
        await sqliteHandle.close();
      }
    }
  }

  /**
   * Load the the JSON file distributed with the release for this blocklist.
   *
   * For Bug 1257565 this method will have to try to load the file from the profile,
   * in order to leverage the updateJSONBlocklist() below, which writes a new
   * dump each time the collection changes.
   */
  async loadDumpFile() {
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

  async validateCollectionSignature(remote, payload, collection, options = {}) {
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
   * Synchronize from Kinto server, if necessary.
   *
   * @param {int}  lastModified     the lastModified date (on the server) for
                                    the remote collection.
   * @param {Date} serverTime       the current date return by the server.
   * @param {Object} options        additional advanced options.
   * @param {bool} options.loadDump load initial dump from disk on first sync (default: true)
   * @return {Promise}              which rejects on sync or process failure.
   */
  async maybeSync(lastModified, serverTime, options = {loadDump: true}) {
    const {loadDump} = options;
    const remote = Services.prefs.getCharPref(PREF_SETTINGS_SERVER);
    const enforceCollectionSigning =
      Services.prefs.getBoolPref(PREF_BLOCKLIST_ENFORCE_SIGNING);

    // if there is a signerName and collection signing is enforced, add a
    // hook for incoming changes that validates the signature
    const colOptions = {};
    if (this.signerName && enforceCollectionSigning) {
      colOptions.hooks = {
        "incoming-changes": [(payload, collection) => {
          return this.validateCollectionSignature(remote, payload, collection);
        }]
      };
    }

    let reportStatus = null;
    try {
      return await this.openCollection(async (collection) => {
        // Synchronize remote data into a local Sqlite DB.
        let collectionLastModified = await collection.db.getLastModified();

        // If there is no data currently in the collection, attempt to import
        // initial data from the application defaults.
        // This allows to avoid synchronizing the whole collection content on
        // cold start.
        if (!collectionLastModified && loadDump) {
          try {
            const initialData = await this.loadDumpFile();
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
          this.updateLastCheck(serverTime);
          reportStatus = UptakeTelemetry.STATUS.UP_TO_DATE;
          return;
        }

        // Fetch changes from server.
        try {
          // Server changes have priority during synchronization.
          const strategy = Kinto.syncStrategy.SERVER_WINS;
          const {ok} = await collection.sync({remote, strategy});
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
              await this.validateCollectionSignature(remote, payload, collection, {ignoreLocal: true});
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
        const {data} = await collection.list();

        // Handle the obtained records (ie. apply locally).
        try {
          await this.processCallback(data);
        } catch (e) {
          reportStatus = UptakeTelemetry.STATUS.APPLY_ERROR;
          throw e;
        }

        // Track last update.
        this.updateLastCheck(serverTime);

      }, colOptions);
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
   * Save last time server was checked in users prefs.
   *
   * @param {Date} serverTime   the current date return by server.
   */
  updateLastCheck(serverTime) {
    const checkedServerTimeInSeconds = Math.round(serverTime / 1000);
    Services.prefs.setIntPref(this.lastCheckTimePref, checkedServerTimeInSeconds);
  }
}

/**
 * Revoke the appropriate certificates based on the records from the blocklist.
 *
 * @param {Object} records   current records in the local db.
 */
async function updateCertBlocklist(records) {
  const certList = Cc["@mozilla.org/security/certblocklist;1"]
                     .getService(Ci.nsICertBlocklist);
  for (let item of records) {
    try {
      if (item.issuerName && item.serialNumber) {
        certList.revokeCertByIssuerAndSerial(item.issuerName,
                                            item.serialNumber);
      } else if (item.subject && item.pubKeyHash) {
        certList.revokeCertBySubjectAndPubKey(item.subject,
                                              item.pubKeyHash);
      }
    } catch (e) {
      // prevent errors relating to individual blocklist entries from
      // causing sync to fail. We will accumulate telemetry on these failures in
      // bug 1254099.
      Cu.reportError(e);
    }
  }
  certList.saveEntries();
}

/**
 * Modify the appropriate security pins based on records from the remote
 * collection.
 *
 * @param {Object} records   current records in the local db.
 */
async function updatePinningList(records) {
  if (!Services.prefs.getBoolPref(PREF_BLOCKLIST_PINNING_ENABLED)) {
    return;
  }
  const appInfo = Cc["@mozilla.org/xre/app-info;1"]
      .getService(Ci.nsIXULAppInfo);

  const siteSecurityService = Cc["@mozilla.org/ssservice;1"]
      .getService(Ci.nsISiteSecurityService);

  // clear the current preload list
  siteSecurityService.clearPreloads();

  // write each KeyPin entry to the preload list
  for (let item of records) {
    try {
      const {pinType, pins = [], versions} = item;
      if (versions.indexOf(appInfo.version) != -1) {
        if (pinType == "KeyPin" && pins.length) {
          siteSecurityService.setKeyPins(item.hostName,
              item.includeSubdomains,
              item.expires,
              pins.length,
              pins, true);
        }
        if (pinType == "STSPin") {
          siteSecurityService.setHSTSPreload(item.hostName,
                                             item.includeSubdomains,
                                             item.expires);
        }
      }
    } catch (e) {
      // prevent errors relating to individual preload entries from causing
      // sync to fail. We will accumulate telemetry for such failures in bug
      // 1254099.
    }
  }
}

/**
 * Write list of records into JSON file, and notify nsBlocklistService.
 *
 * @param {String} filename  path relative to profile dir.
 * @param {Object} records   current records in the local db.
 */
async function updateJSONBlocklist(filename, records) {
  // Write JSON dump for synchronous load at startup.
  const path = OS.Path.join(OS.Constants.Path.profileDir, filename);
  const blocklistFolder = OS.Path.dirname(path);

  await OS.File.makeDir(blocklistFolder, {from: OS.Constants.Path.profileDir});

  const serialized = JSON.stringify({data: records}, null, 2);
  try {
    await OS.File.writeAtomic(path, serialized, {tmpPath: path + ".tmp"});
    // Notify change to `nsBlocklistService`
    const eventData = {filename};
    Services.cpmm.sendAsyncMessage("Blocklist:reload-from-disk", eventData);
  } catch (e) {
    Cu.reportError(e);
  }
}

this.OneCRLBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_BLOCKLIST_ONECRL_COLLECTION),
  PREF_BLOCKLIST_ONECRL_CHECKED_SECONDS,
  updateCertBlocklist,
  Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET),
  "onecrl.content-signature.mozilla.org"
);

this.AddonBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_BLOCKLIST_ADDONS_COLLECTION),
  PREF_BLOCKLIST_ADDONS_CHECKED_SECONDS,
  (records) => updateJSONBlocklist(this.AddonBlocklistClient.filename, records),
  Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET)
);

this.GfxBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_BLOCKLIST_GFX_COLLECTION),
  PREF_BLOCKLIST_GFX_CHECKED_SECONDS,
  (records) => updateJSONBlocklist(this.GfxBlocklistClient.filename, records),
  Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET)
);

this.PluginBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_BLOCKLIST_PLUGINS_COLLECTION),
  PREF_BLOCKLIST_PLUGINS_CHECKED_SECONDS,
  (records) => updateJSONBlocklist(this.PluginBlocklistClient.filename, records),
  Services.prefs.getCharPref(PREF_BLOCKLIST_BUCKET)
);

this.PinningPreloadClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_COLLECTION),
  PREF_BLOCKLIST_PINNING_CHECKED_SECONDS,
  updatePinningList,
  Services.prefs.getCharPref(PREF_BLOCKLIST_PINNING_BUCKET),
  "pinning-preload.content-signature.mozilla.org"
);
