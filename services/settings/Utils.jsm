/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Utils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { ServiceRequest } = ChromeUtils.import(
  "resource://gre/modules/ServiceRequest.jsm"
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

function _isUndefined(value) {
  return typeof value === "undefined";
}

var Utils = {
  get SERVER_URL() {
    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    const isXpcshell = env.exists("XPCSHELL_TEST_PROFILE_DIR");
    const isNotThunderbird = AppConstants.MOZ_APP_NAME != "thunderbird";
    return AppConstants.RELEASE_OR_BETA &&
      !Cu.isInAutomation &&
      !isXpcshell &&
      isNotThunderbird
      ? "https://firefox.settings.services.mozilla.com/v1"
      : gServerURL;
  },

  CHANGES_PATH: "/buckets/monitor/collections/changes/changeset",

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
   * A wrapper around `ServiceRequest` that behaves like `fetch()`.
   * @param input a resource
   * @param init request options
   * @returns a Response object
   */
  async fetch(input, init = {}) {
    return new Promise(function(resolve, reject) {
      const request = new ServiceRequest();

      request.onerror = () =>
        reject(new TypeError("NetworkError: Network request failed"));
      request.ontimeout = () =>
        reject(new TypeError("Timeout: Network request failed"));
      request.onabort = () => reject(new DOMException("Aborted", "AbortError"));
      request.onload = () => {
        // Parse raw response headers into `Headers` object.
        const headers = new Headers();
        const rawHeaders = request.getAllResponseHeaders();
        rawHeaders
          .trim()
          .split(/[\r\n]+/)
          .forEach(line => {
            const parts = line.split(": ");
            const header = parts.shift();
            const value = parts.join(": ");
            headers.set(header, value);
          });

        const responseAttributes = {
          status: request.status,
          statusText: request.statusText,
          url: request.responseURL,
          headers,
        };
        resolve(new Response(request.response, responseAttributes));
      };

      const { method = "GET", headers = {} } = init;

      request.open(method, input, true);

      for (const [name, value] of Object.entries(headers)) {
        request.setRequestHeader(name, value);
      }

      request.send();
    });
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
      await Utils.fetch(
        `resource://app/defaults/settings/${bucket}/${collection}.json`
      );
      return true;
    } catch (e) {
      return false;
    }
  },

  /**
   * Look up the last modification time of the JSON dump.
   *
   * @param {String} bucket
   * @param {String} collection
   * @return {int} The last modification time of the dump. -1 if non-existent.
   */
  async getLocalDumpLastModified(bucket, collection) {
    if (!this._dumpStats) {
      if (!this._dumpStatsInitPromise) {
        this._dumpStatsInitPromise = (async () => {
          try {
            let res = await Utils.fetch(
              "resource://app/defaults/settings/last_modified.json"
            );
            this._dumpStats = await res.json();
          } catch (e) {
            log.warn(`Failed to load last_modified.json: ${e}`);
            this._dumpStats = {};
          }
          delete this._dumpStatsInitPromise;
        })();
      }
      await this._dumpStatsInitPromise;
    }
    const identifier = `${bucket}/${collection}`;
    let lastModified = this._dumpStats[identifier];
    if (lastModified === undefined) {
      try {
        let res = await Utils.fetch(
          `resource://app/defaults/settings/${bucket}/${collection}.json`
        );
        let records = (await res.json()).data;
        // Records in dumps are sorted by last_modified, newest first.
        // https://searchfox.org/mozilla-central/rev/5b3444ad300e244b5af4214212e22bd9e4b7088a/taskcluster/docker/periodic-updates/scripts/periodic_file_updates.sh#304
        lastModified = records[0]?.last_modified || 0;
      } catch (e) {
        lastModified = -1;
      }
      this._dumpStats[identifier] = lastModified;
    }
    return lastModified;
  },

  /**
   * Fetch the list of remote collections and their timestamp.
   * ```
   *   {
   *     "timestamp": 1486545678,
   *     "changes":[
   *       {
   *         "host":"kinto-ota.dev.mozaws.net",
   *         "last_modified":1450717104423,
   *         "bucket":"blocklists",
   *         "collection":"certificates"
   *       },
   *       ...
   *     ],
   *     "metadata": {}
   *   }
   * ```
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

    let url = serverUrl + Utils.CHANGES_PATH;
    const params = {
      ...filters,
      _expected: expectedTimestamp ?? 0,
    };
    if (lastEtag != "") {
      params._since = lastEtag;
    }
    if (params) {
      url +=
        "?" +
        Object.entries(params)
          .map(([k, v]) => `${k}=${encodeURIComponent(v)}`)
          .join("&");
    }
    const response = await Utils.fetch(url);

    if (response.status >= 500) {
      throw new Error(`Server error ${response.status} ${response.statusText}`);
    }

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

    if (!payload.hasOwnProperty("changes")) {
      // If the server is failing, the JSON response might not contain the
      // expected data. For example, real server errors (Bug 1259145)
      // or dummy local server for tests (Bug 1481348)
      if (!is404FromCustomServer) {
        throw new Error(
          `Server error ${url} ${response.status} ${
            response.statusText
          }: ${JSON.stringify(payload)}`
        );
      }
    }

    const { changes = [], timestamp } = payload;

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
      currentEtag: `"${timestamp}"`,
      serverTimeMillis,
      backoffSeconds,
      ageSeconds,
    };
  },

  /**
   * Test if a single object matches all given filters.
   *
   * @param  {Object} filters  The filters object.
   * @param  {Object} entry    The object to filter.
   * @return {Boolean}
   */
  filterObject(filters, entry) {
    return Object.entries(filters).every(([filter, value]) => {
      if (Array.isArray(value)) {
        return value.some(candidate => candidate === entry[filter]);
      } else if (typeof value === "object") {
        return Utils.filterObject(value, entry[filter]);
      } else if (!Object.prototype.hasOwnProperty.call(entry, filter)) {
        console.error(`The property ${filter} does not exist`);
        return false;
      }
      return entry[filter] === value;
    });
  },

  /**
   * Sorts records in a list according to a given ordering.
   *
   * @param  {String} order The ordering, eg. `-last_modified`.
   * @param  {Array}  list  The collection to order.
   * @return {Array}
   */
  sortObjects(order, list) {
    const hasDash = order[0] === "-";
    const field = hasDash ? order.slice(1) : order;
    const direction = hasDash ? -1 : 1;
    return list.slice().sort((a, b) => {
      if (a[field] && _isUndefined(b[field])) {
        return direction;
      }
      if (b[field] && _isUndefined(a[field])) {
        return -direction;
      }
      if (_isUndefined(a[field]) && _isUndefined(b[field])) {
        return 0;
      }
      return a[field] > b[field] ? direction : -direction;
    });
  },
};
