/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 var EXPORTED_SYMBOLS = [
  "Utils",
];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

var Utils = {
  CHANGES_PATH: "/buckets/monitor/collections/changes/records",

  /**
   * Check if local data exist for the specified client.
   *
   * @param {RemoteSettingsClient} client
   * @return {bool} Whether it exists or not.
   */
  async hasLocalData(client) {
    const kintoCol = await client.openCollection();
    const timestamp = await kintoCol.db.getLastModified();
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
      await fetch(`resource://app/defaults/settings/${bucket}/${collection}.json`);
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
      url += "?" + Object.entries(params).map(([k, v]) => `${k}=${encodeURIComponent(v)}`).join("&");
    }
    const response = await fetch(url, { headers });

    let changes = [];
    // If no changes since last time, go on with empty list of changes.
    if (response.status != 304) {
      const ct = response.headers.get("Content-Type");
      if (!ct || !ct.includes("application/json")) {
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
        const is404FromCustomServer = response.status == 404 && Services.prefs.prefHasUserValue("services.settings.server");
        if (!is404FromCustomServer) {
          throw new Error(`Server error ${response.status} ${response.statusText}: ${JSON.stringify(payload)}`);
        }
      } else {
        changes = payload.data;
      }
    }
    // The server should always return ETag. But we've had situations where the CDN
    // was interfering.
    const currentEtag = response.headers.has("ETag") ? response.headers.get("ETag") : undefined;
    let serverTimeMillis = Date.parse(response.headers.get("Date"));
    // Since the response is served via a CDN, the Date header value could have been cached.
    const cacheAgeSeconds = response.headers.has("Age") ? parseInt(response.headers.get("Age"), 10) : 0;
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

    return { changes, currentEtag, serverTimeMillis, backoffSeconds, ageSeconds };
  },
};
