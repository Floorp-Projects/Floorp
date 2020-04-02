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

XPCOMUtils.defineLazyModuleGetters(this, {
  UptakeTelemetry: "resource://services-common/uptake-telemetry.js",
  pushBroadcastService: "resource://gre/modules/PushBroadcastService.jsm",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  Utils: "resource://services-settings/Utils.jsm",
  FilterExpressions:
    "resource://gre/modules/components-utils/FilterExpressions.jsm",
  RemoteSettingsWorker: "resource://services-settings/RemoteSettingsWorker.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const PREF_SETTINGS_DEFAULT_BUCKET = "services.settings.default_bucket";
const PREF_SETTINGS_BRANCH = "services.settings.";
const PREF_SETTINGS_DEFAULT_SIGNER = "default_signer";
const PREF_SETTINGS_SERVER_BACKOFF = "server.backoff";
const PREF_SETTINGS_LAST_UPDATE = "last_update_seconds";
const PREF_SETTINGS_LAST_ETAG = "last_etag";
const PREF_SETTINGS_CLOCK_SKEW_SECONDS = "clock_skew_seconds";
const PREF_SETTINGS_LOAD_DUMP = "load_dump";

// Telemetry identifiers.
const TELEMETRY_COMPONENT = "remotesettings";
const TELEMETRY_SOURCE_POLL = "settings-changes-monitoring";
const TELEMETRY_SOURCE_SYNC = "settings-sync";

// Push broadcast id.
const BROADCAST_ID = "remote-settings/monitor_changes";

// Signer to be used when not specified (see Ci.nsIContentSignatureVerifier).
const DEFAULT_SIGNER = "remote-settings.content-signature.mozilla.org";

XPCOMUtils.defineLazyGetter(this, "gPrefs", () => {
  return Services.prefs.getBranch(PREF_SETTINGS_BRANCH);
});
XPCOMUtils.defineLazyGetter(this, "console", () => Utils.log);

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
    result = await FilterExpressions.eval(filter_expression, context);
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
    bucketNamePref: PREF_SETTINGS_DEFAULT_BUCKET,
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
      const c = new RemoteSettingsClient(collectionName, {
        ...defaultOptions,
        ...options,
      });
      // Store instance for later call.
      _clients.set(collectionName, c);
      // Invalidate the polling status, since we want the new collection to
      // be taken into account.
      _invalidatePolling = true;
      console.debug(`Instantiated new client ${c.identifier}`);
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
      bucketName == Services.prefs.getCharPref(PREF_SETTINGS_DEFAULT_BUCKET)
    ) {
      const c = new RemoteSettingsClient(collectionName, defaultOptions);
      const [dbExists, localDump] = await Promise.all([
        Utils.hasLocalData(c),
        Utils.hasLocalDump(bucketName, collectionName),
      ]);
      if (dbExists || localDump) {
        return c;
      }
    }
    // Else, we cannot return a client insttance because we are not able to synchronize data in specific buckets.
    // Mainly because we cannot guess which `signerName` has to be used for example.
    // And we don't want to synchronize data for collections in the main bucket that are
    // completely unknown (ie. no database and no JSON dump).
    console.debug(`No known client for ${bucketName}/${collectionName}`);
    return null;
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
      gPrefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      gPrefs.clearUserPref(PREF_SETTINGS_LAST_UPDATE);
      gPrefs.clearUserPref(PREF_SETTINGS_LAST_ETAG);
    }

    let pollTelemetryArgs = {
      source: TELEMETRY_SOURCE_POLL,
      trigger,
    };

    if (Utils.isOffline) {
      console.info("Network is offline. Give up.");
      await UptakeTelemetry.report(
        TELEMETRY_COMPONENT,
        UptakeTelemetry.STATUS.NETWORK_OFFLINE_ERROR,
        pollTelemetryArgs
      );
      return;
    }

    const startedAt = new Date();

    // Check if the server backoff time is elapsed.
    if (gPrefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF)) {
      const backoffReleaseTime = gPrefs.getCharPref(
        PREF_SETTINGS_SERVER_BACKOFF
      );
      const remainingMilliseconds =
        parseInt(backoffReleaseTime, 10) - Date.now();
      if (remainingMilliseconds > 0) {
        // Backoff time has not elapsed yet.
        await UptakeTelemetry.report(
          TELEMETRY_COMPONENT,
          UptakeTelemetry.STATUS.BACKOFF,
          pollTelemetryArgs
        );
        throw new Error(
          `Server is asking clients to back off; retry in ${Math.ceil(
            remainingMilliseconds / 1000
          )}s.`
        );
      } else {
        gPrefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      }
    }

    console.info("Start polling for changes");
    Services.obs.notifyObservers(
      null,
      "remote-settings:changes-poll-start",
      JSON.stringify({ expectedTimestamp })
    );

    // Do we have the latest version already?
    // Every time we register a new client, we have to fetch the whole list again.
    const lastEtag = _invalidatePolling
      ? ""
      : gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG, "");

    let pollResult;
    try {
      pollResult = await Utils.fetchLatestChanges(Utils.SERVER_URL, {
        expectedTimestamp,
        lastEtag,
      });
    } catch (e) {
      // Report polling error to Uptake Telemetry.
      let reportStatus;
      if (/JSON\.parse/.test(e.message)) {
        reportStatus = UptakeTelemetry.STATUS.PARSE_ERROR;
      } else if (/content-type/.test(e.message)) {
        reportStatus = UptakeTelemetry.STATUS.CONTENT_ERROR;
      } else if (/Server/.test(e.message)) {
        reportStatus = UptakeTelemetry.STATUS.SERVER_ERROR;
      } else if (/Timeout/.test(e.message)) {
        reportStatus = UptakeTelemetry.STATUS.TIMEOUT_ERROR;
      } else if (/NetworkError/.test(e.message)) {
        reportStatus = UptakeTelemetry.STATUS.NETWORK_ERROR;
      } else {
        reportStatus = UptakeTelemetry.STATUS.UNKNOWN_ERROR;
      }
      await UptakeTelemetry.report(
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
        ? UptakeTelemetry.STATUS.UP_TO_DATE
        : UptakeTelemetry.STATUS.SUCCESS;
    await UptakeTelemetry.report(
      TELEMETRY_COMPONENT,
      reportStatus,
      pollTelemetryArgs
    );

    // Check if the server asked the clients to back off (for next poll).
    if (backoffSeconds) {
      console.info(
        "Server asks clients to backoff for ${backoffSeconds} seconds"
      );
      const backoffReleaseTime = Date.now() + backoffSeconds * 1000;
      gPrefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, backoffReleaseTime);
    }

    // Record new update time and the difference between local and server time.
    // Negative clockDifference means local time is behind server time
    // by the absolute of that value in seconds (positive means it's ahead)
    const clockDifference = Math.floor((Date.now() - serverTimeMillis) / 1000);
    gPrefs.setIntPref(PREF_SETTINGS_CLOCK_SKEW_SECONDS, clockDifference);
    const checkedServerTimeInSeconds = Math.round(serverTimeMillis / 1000);
    gPrefs.setIntPref(PREF_SETTINGS_LAST_UPDATE, checkedServerTimeInSeconds);

    // Should the clients try to load JSON dump? (mainly disabled in tests)
    const loadDump = gPrefs.getBoolPref(PREF_SETTINGS_LOAD_DUMP, true);

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
        await client.maybeSync(last_modified, { loadDump, trigger });

        // Save last time this client was successfully synced.
        Services.prefs.setIntPref(
          client.lastCheckTimePref,
          checkedServerTimeInSeconds
        );
      } catch (e) {
        console.error(e);
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
      await UptakeTelemetry.report(
        TELEMETRY_COMPONENT,
        UptakeTelemetry.STATUS.SYNC_ERROR,
        syncTelemetryArgs
      );
      // Rethrow the first observed error
      throw firstError;
    }

    // Save current Etag for next poll.
    if (currentEtag) {
      gPrefs.setCharPref(PREF_SETTINGS_LAST_ETAG, currentEtag);
    }

    // Report the global synchronization success.
    await UptakeTelemetry.report(
      TELEMETRY_COMPONENT,
      UptakeTelemetry.STATUS.SUCCESS,
      syncTelemetryArgs
    );

    console.info("Polling for changes done");
    Services.obs.notifyObservers(null, "remote-settings:changes-poll-end");
  };

  /**
   * Returns an object with polling status information and the list of
   * known remote settings collections.
   */
  remoteSettings.inspect = async () => {
    const {
      changes,
      currentEtag: serverTimestamp,
    } = await Utils.fetchLatestChanges(Utils.SERVER_URL);

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
      serverURL: Utils.SERVER_URL,
      pollingEndpoint: Utils.SERVER_URL + Utils.CHANGES_PATH,
      serverTimestamp,
      localTimestamp: gPrefs.getCharPref(PREF_SETTINGS_LAST_ETAG, null),
      lastCheck: gPrefs.getIntPref(PREF_SETTINGS_LAST_UPDATE, 0),
      mainBucket: Services.prefs.getCharPref(PREF_SETTINGS_DEFAULT_BUCKET),
      defaultSigner: DEFAULT_SIGNER,
      collections: collections.filter(c => !!c),
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
    console.info("Initialize Remote Settings");
    // Hook the Push broadcast and RemoteSettings polling.
    // When we start on a new profile there will be no ETag stored.
    // Use an arbitrary ETag that is guaranteed not to occur.
    // This will trigger a broadcast message but that's fine because we
    // will check the changes on each collection and retrieve only the
    // changes (e.g. nothing if we have a dump with the same data).
    const currentVersion = gPrefs.getStringPref(PREF_SETTINGS_LAST_ETAG, '"0"');
    const moduleInfo = {
      moduleURI: __URI__,
      symbolName: "remoteSettingsBroadcastHandler",
    };
    pushBroadcastService.addListener(BROADCAST_ID, currentVersion, moduleInfo);
  };

  return remoteSettings;
}

var RemoteSettings = remoteSettingsFunction();

var remoteSettingsBroadcastHandler = {
  async receivedBroadcastMessage(version, broadcastID, context) {
    const { phase } = context;
    const isStartup = [
      pushBroadcastService.PHASES.HELLO,
      pushBroadcastService.PHASES.REGISTER,
    ].includes(phase);

    console.info(
      `Push notification received (version=${version} phase=${phase})`
    );

    return RemoteSettings.pollChanges({
      expectedTimestamp: version,
      trigger: isStartup ? "startup" : "broadcast",
    });
  },
};
