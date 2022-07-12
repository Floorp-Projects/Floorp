/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Utils"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { ServiceRequest } = ChromeUtils.import(
  "resource://gre/modules/ServiceRequest.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  SharedUtils: "resource://services-settings/SharedUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "CaptivePortalService",
  "@mozilla.org/network/captive-portal-service;1",
  "nsICaptivePortalService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
// See LOG_LEVELS in Console.jsm. Common examples: "all", "debug", "info", "warn", "error".
const log = (() => {
  const { ConsoleAPI } = ChromeUtils.import(
    "resource://gre/modules/Console.jsm"
  );
  return new ConsoleAPI({
    maxLogLevel: "warn",
    maxLogLevelPref: "services.settings.loglevel",
    prefix: "services.settings",
  });
})();

XPCOMUtils.defineLazyGetter(lazy, "isRunningTests", () => {
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  if (env.get("MOZ_DISABLE_NONLOCAL_CONNECTIONS") === "1") {
    // Allow to override the server URL if non-local connections are disabled,
    // usually true when running tests.
    return true;
  }
  return false;
});

// Overriding the server URL is normally disabled on Beta and Release channels,
// except under some conditions.
XPCOMUtils.defineLazyGetter(lazy, "allowServerURLOverride", () => {
  if (!AppConstants.RELEASE_OR_BETA) {
    // Always allow to override the server URL on Nightly/DevEdition.
    return true;
  }

  if (lazy.isRunningTests) {
    return true;
  }

  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  if (env.get("MOZ_REMOTE_SETTINGS_DEVTOOLS") === "1") {
    // Allow to override the server URL when using remote settings devtools.
    return true;
  }

  return false;
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gServerURL",
  "services.settings.server",
  AppConstants.REMOTE_SETTINGS_SERVER_URL
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gPreviewEnabled",
  "services.settings.preview_enabled",
  false
);

function _isUndefined(value) {
  return typeof value === "undefined";
}

var Utils = {
  get SERVER_URL() {
    return lazy.allowServerURLOverride
      ? lazy.gServerURL
      : AppConstants.REMOTE_SETTINGS_SERVER_URL;
  },

  CHANGES_PATH: "/buckets/monitor/collections/changes/changeset",

  /**
   * Logger instance.
   */
  log,

  get CERT_CHAIN_ROOT_IDENTIFIER() {
    if (this.SERVER_URL == AppConstants.REMOTE_SETTINGS_SERVER_URL) {
      return Ci.nsIContentSignatureVerifier.ContentSignatureProdRoot;
    }
    if (this.SERVER_URL.includes("stage.")) {
      return Ci.nsIContentSignatureVerifier.ContentSignatureStageRoot;
    }
    if (this.SERVER_URL.includes("dev.")) {
      return Ci.nsIContentSignatureVerifier.ContentSignatureDevRoot;
    }
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    if (env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      return Ci.nsIX509CertDB.AppXPCShellRoot;
    }
    return Ci.nsIContentSignatureVerifier.ContentSignatureLocalRoot;
  },

  get LOAD_DUMPS() {
    // Load dumps only if pulling data from the production server, or in tests.
    return (
      this.SERVER_URL == AppConstants.REMOTE_SETTINGS_SERVER_URL ||
      lazy.isRunningTests
    );
  },

  get PREVIEW_MODE() {
    // We want to offer the ability to set preview mode via a preference
    // for consumers who want to pull from the preview bucket on startup.
    if (_isUndefined(this._previewModeEnabled) && lazy.allowServerURLOverride) {
      return lazy.gPreviewEnabled;
    }
    return !!this._previewModeEnabled;
  },

  /**
   * Internal method to enable pulling data from preview buckets.
   * @param enabled
   */
  enablePreviewMode(enabled) {
    const bool2str = v =>
      // eslint-disable-next-line no-nested-ternary
      _isUndefined(v) ? "unset" : v ? "enabled" : "disabled";
    this.log.debug(
      `Preview mode: ${bool2str(this._previewModeEnabled)} -> ${bool2str(
        enabled
      )}`
    );
    this._previewModeEnabled = enabled;
  },

  /**
   * Returns the actual bucket name to be used. When preview mode is enabled,
   * this adds the *preview* suffix.
   *
   * See also `SharedUtils.loadJSONDump()` which strips the preview suffix to identify
   * the packaged JSON file.
   *
   * @param bucketName the client bucket
   * @returns the final client bucket depending whether preview mode is enabled.
   */
  actualBucketName(bucketName) {
    let actual = bucketName.replace("-preview", "");
    if (this.PREVIEW_MODE) {
      actual += "-preview";
    }
    return actual;
  },

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
        lazy.CaptivePortalService.state ==
          lazy.CaptivePortalService.LOCKED_PORTAL ||
        !lazy.gNetworkLinkService.isLinkUp
      );
    } catch (ex) {
      log.warn("Could not determine network status.", ex);
    }
    return false;
  },

  /**
   * A wrapper around `ServiceRequest` that behaves like `fetch()`.
   *
   * Use this in order to leverage the `beConservative` flag, for
   * example to avoid using HTTP3 to fetch critical data.
   *
   * @param input a resource
   * @param init request options
   * @returns a Response object
   */
  async fetch(input, init = {}) {
    return new Promise(function(resolve, reject) {
      const request = new ServiceRequest();
      function fallbackOrReject(err) {
        if (
          // At most one recursive Utils.fetch call (bypassProxy=false to true).
          bypassProxy ||
          Services.startup.shuttingDown ||
          Utils.isOffline ||
          !request.isProxied ||
          !request.bypassProxyEnabled
        ) {
          reject(err);
          return;
        }
        ServiceRequest.logProxySource(request.channel, "remote-settings");
        resolve(Utils.fetch(input, { ...init, bypassProxy: true }));
      }

      request.onerror = () =>
        fallbackOrReject(new TypeError("NetworkError: Network request failed"));
      request.ontimeout = () =>
        fallbackOrReject(new TypeError("Timeout: Network request failed"));
      request.onabort = () =>
        fallbackOrReject(new DOMException("Aborted", "AbortError"));
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

      const { method = "GET", headers = {}, bypassProxy = false } = init;

      request.open(method, input, { bypassProxy });
      // By default, XMLHttpRequest converts the response based on the
      // Content-Type header, or UTF-8 otherwise. This may mangle binary
      // responses. Avoid that by requesting the raw bytes.
      request.responseType = "arraybuffer";

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
        `resource://app/defaults/settings/${bucket}/${collection}.json`,
        {
          method: "HEAD",
        }
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
            let res = await fetch(
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
      const { timestamp: dumpTimestamp } = await lazy.SharedUtils.loadJSONDump(
        bucket,
        collection
      );
      // Client recognize -1 as missing dump.
      lastModified = dumpTimestamp ?? -1;
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
    const ageSeconds = (serverTimeMillis - timestamp) / 1000;

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
