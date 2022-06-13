/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global __URI__ */

"use strict";

var EXPORTED_SYMBOLS = [
  "RemoteSettings",
  "jexlFilterFunc",
  "remoteSettingsBroadcastHandler",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  UptakeTelemetry: "resource://services-common/uptake-telemetry.js",
  pushBroadcastService: "resource://gre/modules/PushBroadcastService.jsm",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  SyncHistory: "resource://services-settings/SyncHistory.jsm",
  Database: "resource://services-settings/Database.jsm",
  Utils: "resource://services-settings/Utils.jsm",
  FilterExpressions:
    "resource://gre/modules/components-utils/FilterExpressions.jsm",
  RemoteSettingsWorker: "resource://services-settings/RemoteSettingsWorker.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(lazy, ["fetch"]);

const PREF_SETTINGS_BRANCH = "services.settings.";
const PREF_SETTINGS_SERVER_BACKOFF = "server.backoff";
const PREF_SETTINGS_LAST_UPDATE = "last_update_seconds";
const PREF_SETTINGS_LAST_ETAG = "last_etag";
const PREF_SETTINGS_CLOCK_SKEW_SECONDS = "clock_skew_seconds";
const PREF_SETTINGS_SYNC_HISTORY_SIZE = "sync_history_size";
const PREF_SETTINGS_SYNC_HISTORY_ERROR_THRESHOLD =
  "sync_history_error_threshold";

// Telemetry identifiers.
const TELEMETRY_COMPONENT = "remotesettings";
const TELEMETRY_SOURCE_POLL = "settings-changes-monitoring";
const TELEMETRY_SOURCE_SYNC = "settings-sync";

// Push broadcast id.
const BROADCAST_ID = "remote-settings/monitor_changes";

// Signer to be used when not specified (see Ci.nsIContentSignatureVerifier).
const DEFAULT_SIGNER = "remote-settings.content-signature.mozilla.org";

XPCOMUtils.defineLazyGetter(lazy, "gPrefs", () => {
  return Services.prefs.getBranch(PREF_SETTINGS_BRANCH);
});
XPCOMUtils.defineLazyGetter(lazy, "console", () => lazy.Utils.log);

XPCOMUtils.defineLazyGetter(lazy, "gSyncHistory", () => {
  const prefSize = lazy.gPrefs.getIntPref(PREF_SETTINGS_SYNC_HISTORY_SIZE, 100);
  const size = Math.min(Math.max(prefSize, 1000), 10);
  return new lazy.SyncHistory(TELEMETRY_SOURCE_SYNC, { size });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gPrefBrokenSyncThreshold",
  PREF_SETTINGS_BRANCH + PREF_SETTINGS_SYNC_HISTORY_ERROR_THRESHOLD,
  10
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gPrefDestroyBrokenEnabled",
  PREF_SETTINGS_BRANCH + "destroy_broken_db_enabled",
  true
);

/**
 * Default entry filtering function, in charge of excluding remote settings entries
 * where the JEXL expression evaluates into a falsy value.
 * @param {Object}            entry       The Remote Settings entry to be excluded or kept.
 * @param {ClientEnvironment} environment Information about version, language, platform etc.
 * @returns {?Object} the entry or null if excluded.
 */
async function jexlFilterFunc(entry, environment) {
  const { filter_expression } = entry;
  if (!filter_expression) {
    return entry;
  }
  let result;
  try {
    const context = {
      env: environment,
    };
    result = await lazy.FilterExpressions.eval(filter_expression, context);
  } catch (e) {
    Cu.reportError(e);
  }
  return result ? entry : null;
}

function remoteSettingsFunction() {
  const _clients = new Map();
  let _invalidatePolling = false;

  // If not explicitly specified, use the default signer.
  const defaultOptions = {
    signerName: DEFAULT_SIGNER,
    filterFunc: jexlFilterFunc,
  };

  /**
   * RemoteSettings constructor.
   *
   * @param {String} collectionName The remote settings identifier
   * @param {Object} options Advanced options
   * @returns {RemoteSettingsClient} An instance of a Remote Settings client.
   */
  const remoteSettings = function(collectionName, options) {
    // Get or instantiate a remote settings client.
    if (!_clients.has(collectionName)) {
      // Register a new client!
      const c = new lazy.RemoteSettingsClient(collectionName, {
        ...defaultOptions,
        ...options,
      });
      // Store instance for later call.
      _clients.set(collectionName, c);
      // Invalidate the polling status, since we want the new collection to
      // be taken into account.
      _invalidatePolling = true;
      lazy.console.debug(`Instantiated new client ${c.identifier}`);
    }
    return _clients.get(collectionName);
  };

  /**
   * Internal helper to retrieve existing instances of clients or new instances
   * with default options if possible, or `null` if bucket/collection are unknown.
   */
  async function _client(bucketName, collectionName) {
    // Check if a client was registered for this bucket/collection. Potentially
    // with some specific options like signer, filter function etc.
    const client = _clients.get(collectionName);
    if (client && client.bucketName == bucketName) {
      return client;
    }
    // There was no client registered for this collection, but it's the main bucket,
    // therefore we can instantiate a client with the default options.
    // So if we have a local database or if we ship a JSON dump, then it means that
    // this client is known but it was not registered yet (eg. calling module not "imported" yet).
    if (
      bucketName ==
      lazy.Utils.actualBucketName(AppConstants.REMOTE_SETTINGS_DEFAULT_BUCKET)
    ) {
      const c = new lazy.RemoteSettingsClient(collectionName, defaultOptions);
      const [dbExists, localDump] = await Promise.all([
        lazy.Utils.hasLocalData(c),
        lazy.Utils.hasLocalDump(bucketName, collectionName),
      ]);
      if (dbExists || localDump) {
        return c;
      }
    }
    // Else, we cannot return a client instance because we are not able to synchronize data in specific buckets.
    // Mainly because we cannot guess which `signerName` has to be used for example.
    // And we don't want to synchronize data for collections in the main bucket that are
    // completely unknown (ie. no database and no JSON dump).
    lazy.console.debug(`No known client for ${bucketName}/${collectionName}`);
    return null;
  }

  /**
   * Helper to introspect the synchronization history and determine whether it is
   * consistently failing and thus, broken.
   * @returns {bool} true if broken.
   */
  async function isSynchronizationBroken() {
    // The minimum number of errors is customizable, but with a maximum.
    const threshold = Math.min(lazy.gPrefBrokenSyncThreshold, 20);
    // Read history of synchronization past statuses.
    const pastEntries = await lazy.gSyncHistory.list();
    const lastSuccessIdx = pastEntries.findIndex(
      e => e.status == lazy.UptakeTelemetry.STATUS.SUCCESS
    );
    return (
      // Only errors since last success.
      lastSuccessIdx >= threshold ||
      // Or only errors with a minimum number of history entries.
      (lastSuccessIdx < 0 && pastEntries.length >= threshold)
    );
  }

  /**
   * Main polling method, called by the ping mechanism.
   *
   * @param {Object} options
.  * @param {Object} options.expectedTimestamp (optional) The expected timestamp to be received — used by servers for cache busting.
   * @param {string} options.trigger           (optional) label to identify what triggered this sync (eg. ``"timer"``, default: `"manual"`)
   * @param {bool}   options.full              (optional) Ignore last polling status and fetch all changes (default: `false`)
   * @returns {Promise} or throws error if something goes wrong.
   */
  remoteSettings.pollChanges = async ({
    expectedTimestamp,
    trigger = "manual",
    full = false,
  } = {}) => {
    // When running in full mode, we ignore last polling status.
    if (full) {
      lazy.gPrefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      lazy.gPrefs.clearUserPref(PREF_SETTINGS_LAST_UPDATE);
      lazy.gPrefs.clearUserPref(PREF_SETTINGS_LAST_ETAG);
    }

    let pollTelemetryArgs = {
      source: TELEMETRY_SOURCE_POLL,
      trigger,
    };

    if (lazy.Utils.isOffline) {
      lazy.console.info("Network is offline. Give up.");
      await lazy.UptakeTelemetry.report(
        TELEMETRY_COMPONENT,
        lazy.UptakeTelemetry.STATUS.NETWORK_OFFLINE_ERROR,
        pollTelemetryArgs
      );
      return;
    }

    const startedAt = new Date();

    // Check if the server backoff time is elapsed.
    if (lazy.gPrefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF)) {
      const backoffReleaseTime = lazy.gPrefs.getCharPref(
        PREF_SETTINGS_SERVER_BACKOFF
      );
      const remainingMilliseconds =
        parseInt(backoffReleaseTime, 10) - Date.now();
      if (remainingMilliseconds > 0) {
        // Backoff time has not elapsed yet.
        await lazy.UptakeTelemetry.report(
          TELEMETRY_COMPONENT,
          lazy.UptakeTelemetry.STATUS.BACKOFF,
          pollTelemetryArgs
        );
        throw new Error(
          `Server is asking clients to back off; retry in ${Math.ceil(
            remainingMilliseconds / 1000
          )}s.`
        );
      } else {
        lazy.gPrefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      }
    }

    // When triggered from the daily timer, we try to recover a broken
    // sync state by destroying the local DB completely and retrying from scratch.
    if (
      lazy.gPrefDestroyBrokenEnabled &&
      trigger == "timer" &&
      (await isSynchronizationBroken())
    ) {
      // We don't want to destroy the local DB if the failures are related to
      // network or server errors though.
      const lastStatus = await lazy.gSyncHistory.last();
      const lastErrorClass =
        lazy.RemoteSettingsClient[lastStatus?.infos?.errorName] || Error;
      const isLocalError = !(
        lastErrorClass.prototype instanceof lazy.RemoteSettingsClient.APIError
      );
      if (isLocalError) {
        console.warn(
          "Synchronization has failed consistently. Destroy database."
        );
        // Clear the last ETag to refetch everything.
        lazy.gPrefs.clearUserPref(PREF_SETTINGS_LAST_ETAG);
        // Clear the history, to avoid re-destroying several times in a row.
        await lazy.gSyncHistory.clear().catch(error => Cu.reportError(error));
        // Delete the whole IndexedDB database.
        await lazy.Database.destroy().catch(error => Cu.reportError(error));
      } else {
        console.warn(
          `Synchronization is broken, but last error is ${lastStatus}`
        );
      }
    }

    lazy.console.info("Start polling for changes");
    Services.obs.notifyObservers(
      null,
      "remote-settings:changes-poll-start",
      JSON.stringify({ expectedTimestamp })
    );

    // Do we have the latest version already?
    // Every time we register a new client, we have to fetch the whole list again.
    const lastEtag = _invalidatePolling
      ? ""
      : lazy.gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG, "");

    let pollResult;
    try {
      pollResult = await lazy.Utils.fetchLatestChanges(lazy.Utils.SERVER_URL, {
        expectedTimestamp,
        lastEtag,
      });
    } catch (e) {
      // Report polling error to Uptake Telemetry.
      let reportStatus;
      if (/JSON\.parse/.test(e.message)) {
        reportStatus = lazy.UptakeTelemetry.STATUS.PARSE_ERROR;
      } else if (/content-type/.test(e.message)) {
        reportStatus = lazy.UptakeTelemetry.STATUS.CONTENT_ERROR;
      } else if (/Server/.test(e.message)) {
        reportStatus = lazy.UptakeTelemetry.STATUS.SERVER_ERROR;
      } else if (/Timeout/.test(e.message)) {
        reportStatus = lazy.UptakeTelemetry.STATUS.TIMEOUT_ERROR;
      } else if (/NetworkError/.test(e.message)) {
        reportStatus = lazy.UptakeTelemetry.STATUS.NETWORK_ERROR;
      } else {
        reportStatus = lazy.UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      await lazy.UptakeTelemetry.report(
        TELEMETRY_COMPONENT,
        reportStatus,
        pollTelemetryArgs
      );
      // No need to go further.
      throw new Error(`Polling for changes failed: ${e.message}.`);
    }

    const {
      serverTimeMillis,
      changes,
      currentEtag,
      backoffSeconds,
      ageSeconds,
    } = pollResult;

    // Report age of server data in Telemetry.
    pollTelemetryArgs = { age: ageSeconds, ...pollTelemetryArgs };

    // Report polling success to Uptake Telemetry.
    const reportStatus =
      changes.length === 0
        ? lazy.UptakeTelemetry.STATUS.UP_TO_DATE
        : lazy.UptakeTelemetry.STATUS.SUCCESS;
    await lazy.UptakeTelemetry.report(
      TELEMETRY_COMPONENT,
      reportStatus,
      pollTelemetryArgs
    );

    // Check if the server asked the clients to back off (for next poll).
    if (backoffSeconds) {
      lazy.console.info(
        "Server asks clients to backoff for ${backoffSeconds} seconds"
      );
      const backoffReleaseTime = Date.now() + backoffSeconds * 1000;
      lazy.gPrefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, backoffReleaseTime);
    }

    // Record new update time and the difference between local and server time.
    // Negative clockDifference means local time is behind server time
    // by the absolute of that value in seconds (positive means it's ahead)
    const clockDifference = Math.floor((Date.now() - serverTimeMillis) / 1000);
    lazy.gPrefs.setIntPref(PREF_SETTINGS_CLOCK_SKEW_SECONDS, clockDifference);
    const checkedServerTimeInSeconds = Math.round(serverTimeMillis / 1000);
    lazy.gPrefs.setIntPref(
      PREF_SETTINGS_LAST_UPDATE,
      checkedServerTimeInSeconds
    );

    // Iterate through the collections version info and initiate a synchronization
    // on the related remote settings clients.
    let firstError;
    for (const change of changes) {
      const { bucket, collection, last_modified } = change;

      const client = await _client(bucket, collection);
      if (!client) {
        // This collection has no associated client (eg. preview, other platform...)
        continue;
      }
      // Start synchronization! It will be a no-op if the specified `lastModified` equals
      // the one in the local database.
      try {
        await client.maybeSync(last_modified, { trigger });

        // Save last time this client was successfully synced.
        Services.prefs.setIntPref(
          client.lastCheckTimePref,
          checkedServerTimeInSeconds
        );
      } catch (e) {
        lazy.console.error(e);
        if (!firstError) {
          firstError = e;
          firstError.details = change;
        }
      }
    }

    // Polling is done.
    _invalidatePolling = false;

    // Report total synchronization duration to Telemetry.
    const durationMilliseconds = new Date() - startedAt;
    const syncTelemetryArgs = {
      source: TELEMETRY_SOURCE_SYNC,
      duration: durationMilliseconds,
      timestamp: `${currentEtag}`,
      trigger,
    };

    if (firstError) {
      // Report the global synchronization failure. Individual uptake reports will also have been sent for each collection.
      const status = lazy.UptakeTelemetry.STATUS.SYNC_ERROR;
      await lazy.UptakeTelemetry.report(
        TELEMETRY_COMPONENT,
        status,
        syncTelemetryArgs
      );
      // Keep track of sync failure in history.
      await lazy.gSyncHistory
        .store(currentEtag, status, {
          expectedTimestamp,
          errorName: firstError.name,
        })
        .catch(error => Cu.reportError(error));
      // Notify potential observers of the error.
      Services.obs.notifyObservers(
        { wrappedJSObject: { error: firstError } },
        "remote-settings:sync-error"
      );

      // If synchronization has been consistently failing, send a specific signal.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1729400
      // and https://bugzilla.mozilla.org/show_bug.cgi?id=1658597
      if (await isSynchronizationBroken()) {
        await lazy.UptakeTelemetry.report(
          TELEMETRY_COMPONENT,
          lazy.UptakeTelemetry.STATUS.SYNC_BROKEN_ERROR,
          syncTelemetryArgs
        );

        Services.obs.notifyObservers(
          { wrappedJSObject: { error: firstError } },
          "remote-settings:broken-sync-error"
        );
      }

      // Rethrow the first observed error
      throw firstError;
    }

    // Save current Etag for next poll.
    lazy.gPrefs.setCharPref(PREF_SETTINGS_LAST_ETAG, currentEtag);

    // Report the global synchronization success.
    const status = lazy.UptakeTelemetry.STATUS.SUCCESS;
    await lazy.UptakeTelemetry.report(
      TELEMETRY_COMPONENT,
      status,
      syncTelemetryArgs
    );
    // Keep track of sync success in history.
    await lazy.gSyncHistory
      .store(currentEtag, status)
      .catch(error => Cu.reportError(error));

    lazy.console.info("Polling for changes done");
    Services.obs.notifyObservers(null, "remote-settings:changes-poll-end");
  };

  /**
   * Enables or disables preview mode.
   *
   * When enabled, all existing and future clients will pull data from
   * the `*-preview` buckets. This allows developers and QA to test their
   * changes before publishing them for all clients.
   */
  remoteSettings.enablePreviewMode = enabled => {
    // Set the flag for future clients.
    lazy.Utils.enablePreviewMode(enabled);
    // Enable it on existing clients.
    for (const client of _clients.values()) {
      client.refreshBucketName();
    }
  };

  /**
   * Returns an object with polling status information and the list of
   * known remote settings collections.
   */
  remoteSettings.inspect = async () => {
    const {
      changes,
      currentEtag: serverTimestamp,
    } = await lazy.Utils.fetchLatestChanges(lazy.Utils.SERVER_URL);

    const collections = await Promise.all(
      changes.map(async change => {
        const { bucket, collection, last_modified: serverTimestamp } = change;
        const client = await _client(bucket, collection);
        if (!client) {
          return null;
        }
        const localTimestamp = await client.getLastModified();
        const lastCheck = Services.prefs.getIntPref(
          client.lastCheckTimePref,
          0
        );
        return {
          bucket,
          collection,
          localTimestamp,
          serverTimestamp,
          lastCheck,
          signerName: client.signerName,
        };
      })
    );

    return {
      serverURL: lazy.Utils.SERVER_URL,
      pollingEndpoint: lazy.Utils.SERVER_URL + lazy.Utils.CHANGES_PATH,
      serverTimestamp,
      localTimestamp: lazy.gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG, null),
      lastCheck: lazy.gPrefs.getIntPref(PREF_SETTINGS_LAST_UPDATE, 0),
      mainBucket: lazy.Utils.actualBucketName(
        AppConstants.REMOTE_SETTINGS_DEFAULT_BUCKET
      ),
      defaultSigner: DEFAULT_SIGNER,
      previewMode: lazy.Utils.PREVIEW_MODE,
      collections: collections.filter(c => !!c),
      history: {
        [TELEMETRY_SOURCE_SYNC]: await lazy.gSyncHistory.list(),
      },
    };
  };

  /**
   * Delete all local data, of every collection.
   */
  remoteSettings.clearAll = async () => {
    const { collections } = await remoteSettings.inspect();
    await Promise.all(
      collections.map(async ({ collection }) => {
        const client = RemoteSettings(collection);
        // Delete all potential attachments.
        await client.attachments.deleteAll();
        // Delete local data.
        await client.db.clear();
        // Remove status pref.
        Services.prefs.clearUserPref(client.lastCheckTimePref);
      })
    );
  };

  /**
   * Startup function called from nsBrowserGlue.
   */
  remoteSettings.init = () => {
    lazy.console.info("Initialize Remote Settings");
    // Hook the Push broadcast and RemoteSettings polling.
    // When we start on a new profile there will be no ETag stored.
    // Use an arbitrary ETag that is guaranteed not to occur.
    // This will trigger a broadcast message but that's fine because we
    // will check the changes on each collection and retrieve only the
    // changes (e.g. nothing if we have a dump with the same data).
    const currentVersion = lazy.gPrefs.getStringPref(
      PREF_SETTINGS_LAST_ETAG,
      '"0"'
    );
    const moduleInfo = {
      moduleURI: __URI__,
      symbolName: "remoteSettingsBroadcastHandler",
    };
    lazy.pushBroadcastService.addListener(
      BROADCAST_ID,
      currentVersion,
      moduleInfo
    );
  };

  return remoteSettings;
}

var RemoteSettings = remoteSettingsFunction();

var remoteSettingsBroadcastHandler = {
  async receivedBroadcastMessage(version, broadcastID, context) {
    const { phase } = context;
    const isStartup = [
      lazy.pushBroadcastService.PHASES.HELLO,
      lazy.pushBroadcastService.PHASES.REGISTER,
    ].includes(phase);

    lazy.console.info(
      `Push notification received (version=${version} phase=${phase})`
    );

    return RemoteSettings.pollChanges({
      expectedTimestamp: version,
      trigger: isStartup ? "startup" : "broadcast",
    });
  },
};
