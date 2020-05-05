/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Utils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "CaptivePortalService",
  "@mozilla.org/network/captive-portal-service;1",
  "nsICaptivePortalService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
// See LOG_LEVELS in Console.jsm. Common examples: "all", "debug", "info", "warn", "error".
XPCOMUtils.defineLazyGetter(this, "log", () => {
  const { ConsoleAPI } = ChromeUtils.import(
    "resource://gre/modules/Console.jsm",
    {}
  );
  return new ConsoleAPI({
    maxLogLevel: "warn",
    maxLogLevelPref: "services.settings.loglevel",
    prefix: "services.settings",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gServerURL",
  "services.settings.server"
);

var Utils = {
  get SERVER_URL() {
    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    const isXpcshell = env.exists("XPCSHELL_TEST_PROFILE_DIR");
    return AppConstants.RELEASE_OR_BETA && !Cu.isInAutomation && !isXpcshell
      ? "https://firefox.settings.services.mozilla.com/v1"
      : gServerURL;
  },

  CHANGES_PATH: "/buckets/monitor/collections/changes/records",

  /**
   * Logger instance.
   */
  log,

  /**
   * Check if network is down.
   *
   * Note that if this returns false, it does not guarantee
   * that network is up.
   *
   * @return {bool} Whether network is down or not.
   */
  get isOffline() {
    try {
      return (
        Services.io.offline ||
        CaptivePortalService.state == CaptivePortalService.LOCKED_PORTAL ||
        !gNetworkLinkService.isLinkUp
      );
    } catch (ex) {
      log.warn("Could not determine network status.", ex);
    }
    return false;
  },

  /**
   * Check if local data exist for the specified client.
   *
   * @param {RemoteSettingsClient} client
   * @return {bool} Whether it exists or not.
   */
  async hasLocalData(client) {
    const timestamp = await client.db.getLastModified();
    // Note: timestamp will be 0 if empty JSON dump is loaded.
    return timestamp !== null;
  },

  /**
   * Check if we ship a JSON dump for the specified bucket and collection.
   *
   * @param {String} bucket
   * @param {String} collection
   * @return {bool} Whether it is present or not.
   */
  async hasLocalDump(bucket, collection) {
    try {
      await fetch(
        `resource://app/defaults/settings/${bucket}/${collection}.json`
      );
      return true;
    } catch (e) {
      return false;
    }
  },

  /**
   * Fetch the list of remote collections and their timestamp.
   * @param {String} serverUrl         The server URL (eg. `https://server.org/v1`)
   * @param {int}    expectedTimestamp The timestamp that the server is supposed to return.
   *                                   We obtained it from the Megaphone notification payload,
   *                                   and we use it only for cache busting (Bug 1497159).
   * @param {String} lastEtag          (optional) The Etag of the latest poll to be matched
   *                                   by the server (eg. `"123456789"`).
   * @param {Object} filters
   */
  async fetchLatestChanges(serverUrl, options = {}) {
    const { expectedTimestamp, lastEtag = "", filters = {} } = options;

    //
    // Fetch the list of changes objects from the server that looks like:
    // {"data":[
    //   {
    //     "host":"kinto-ota.dev.mozaws.net",
    //     "last_modified":1450717104423,
    //     "bucket":"blocklists",
    //     "collection":"certificates"
    //    }]}

    let url = serverUrl + Utils.CHANGES_PATH;

    // Use ETag to obtain a `304 Not modified` when no change occurred,
    // and `?_since` parameter to only keep entries that weren't processed yet.
    const headers = {};
    const params = { ...filters };
    if (lastEtag != "") {
      headers["If-None-Match"] = lastEtag;
      params._since = lastEtag;
    }
    if (expectedTimestamp) {
      params._expected = expectedTimestamp;
    }
    if (params) {
      url +=
        "?" +
        Object.entries(params)
          .map(([k, v]) => `${k}=${encodeURIComponent(v)}`)
          .join("&");
    }
    const response = await fetch(url, { headers });

    let changes = [];
    // If no changes since last time, go on with empty list of changes.
    if (response.status != 304) {
      const is404FromCustomServer =
        response.status == 404 &&
        Services.prefs.prefHasUserValue("services.settings.server");

      const ct = response.headers.get("Content-Type");
      if (!is404FromCustomServer && (!ct || !ct.includes("application/json"))) {
        throw new Error(`Unexpected content-type "${ct}"`);
      }
      let payload;
      try {
        payload = await response.json();
      } catch (e) {
        payload = e.message;
      }

      if (!payload.hasOwnProperty("data")) {
        // If the server is failing, the JSON response might not contain the
        // expected data. For example, real server errors (Bug 1259145)
        // or dummy local server for tests (Bug 1481348)
        if (!is404FromCustomServer) {
          throw new Error(
            `Server error ${response.status} ${
              response.statusText
            }: ${JSON.stringify(payload)}`
          );
        }
      } else {
        changes = payload.data;
      }
    }
    // The server should always return ETag. But we've had situations where the CDN
    // was interfering.
    const currentEtag = response.headers.has("ETag")
      ? response.headers.get("ETag")
      : undefined;
    let serverTimeMillis = Date.parse(response.headers.get("Date"));
    // Since the response is served via a CDN, the Date header value could have been cached.
    const cacheAgeSeconds = response.headers.has("Age")
      ? parseInt(response.headers.get("Age"), 10)
      : 0;
    serverTimeMillis += cacheAgeSeconds * 1000;

    // Age of data (time between publication and now).
    let lastModifiedMillis = Date.parse(response.headers.get("Last-Modified"));
    const ageSeconds = (serverTimeMillis - lastModifiedMillis) / 1000;

    // Check if the server asked the clients to back off.
    let backoffSeconds;
    if (response.headers.has("Backoff")) {
      const value = parseInt(response.headers.get("Backoff"), 10);
      if (!isNaN(value)) {
        backoffSeconds = value;
      }
    }

    return {
      changes,
      currentEtag,
      serverTimeMillis,
      backoffSeconds,
      ageSeconds,
    };
  },
};
