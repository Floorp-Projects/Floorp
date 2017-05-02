/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["checkVersions", "addTestBlocklistClient"];

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.importGlobalProperties(["fetch"]);
const BlocklistClients = Cu.import("resource://services-common/blocklist-clients.js", {});

const PREF_SETTINGS_SERVER              = "services.settings.server";
const PREF_SETTINGS_SERVER_BACKOFF      = "services.settings.server.backoff";
const PREF_BLOCKLIST_CHANGES_PATH       = "services.blocklist.changes.path";
const PREF_BLOCKLIST_LAST_UPDATE        = "services.blocklist.last_update_seconds";
const PREF_BLOCKLIST_LAST_ETAG          = "services.blocklist.last_etag";
const PREF_BLOCKLIST_CLOCK_SKEW_SECONDS = "services.blocklist.clock_skew_seconds";


const gBlocklistClients = {
  [BlocklistClients.OneCRLBlocklistClient.collectionName]: BlocklistClients.OneCRLBlocklistClient,
  [BlocklistClients.AddonBlocklistClient.collectionName]: BlocklistClients.AddonBlocklistClient,
  [BlocklistClients.GfxBlocklistClient.collectionName]: BlocklistClients.GfxBlocklistClient,
  [BlocklistClients.PluginBlocklistClient.collectionName]: BlocklistClients.PluginBlocklistClient,
  [BlocklistClients.PinningPreloadClient.collectionName]: BlocklistClients.PinningPreloadClient
};

// Add a blocklist client for testing purposes. Do not use for any other purpose
this.addTestBlocklistClient = (name, client) => { gBlocklistClients[name] = client; }


// This is called by the ping mechanism.
// returns a promise that rejects if something goes wrong
this.checkVersions = function() {
  return Task.spawn(function* syncClients() {

    // Check if the server backoff time is elapsed.
    if (Services.prefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF)) {
      const backoffReleaseTime = Services.prefs.getCharPref(PREF_SETTINGS_SERVER_BACKOFF);
      const remainingMilliseconds = parseInt(backoffReleaseTime, 10) - Date.now();
      if (remainingMilliseconds > 0) {
        throw new Error(`Server is asking clients to back off; retry in ${Math.ceil(remainingMilliseconds / 1000)}s.`);
      } else {
        Services.prefs.clearUserPref(PREF_SETTINGS_SERVER_BACKOFF);
      }
    }

    // Fetch a versionInfo object that looks like:
    // {"data":[
    //   {
    //     "host":"kinto-ota.dev.mozaws.net",
    //     "last_modified":1450717104423,
    //     "bucket":"blocklists",
    //     "collection":"certificates"
    //    }]}
    // Right now, we only use the collection name and the last modified info
    const kintoBase = Services.prefs.getCharPref(PREF_SETTINGS_SERVER);
    const changesEndpoint = kintoBase + Services.prefs.getCharPref(PREF_BLOCKLIST_CHANGES_PATH);

    // Use ETag to obtain a `304 Not modified` when no change occurred.
    const headers = {};
    if (Services.prefs.prefHasUserValue(PREF_BLOCKLIST_LAST_ETAG)) {
      const lastEtag = Services.prefs.getCharPref(PREF_BLOCKLIST_LAST_ETAG);
      if (lastEtag) {
        headers["If-None-Match"] = lastEtag;
      }
    }

    const response = yield fetch(changesEndpoint, {headers});

    // Check if the server asked the clients to back off.
    if (response.headers.has("Backoff")) {
      const backoffSeconds = parseInt(response.headers.get("Backoff"), 10);
      if (!isNaN(backoffSeconds)) {
        const backoffReleaseTime = Date.now() + backoffSeconds * 1000;
        Services.prefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, backoffReleaseTime);
      }
    }

    let versionInfo;
    // No changes since last time. Go on with empty list of changes.
    if (response.status == 304) {
      versionInfo = {data: []};
    } else {
      versionInfo = yield response.json();
    }

    // If the server is failing, the JSON response might not contain the
    // expected data (e.g. error response - Bug 1259145)
    if (!versionInfo.hasOwnProperty("data")) {
      throw new Error("Polling for changes failed.");
    }

    // Record new update time and the difference between local and server time
    const serverTimeMillis = Date.parse(response.headers.get("Date"));

    // negative clockDifference means local time is behind server time
    // by the absolute of that value in seconds (positive means it's ahead)
    const clockDifference = Math.floor((Date.now() - serverTimeMillis) / 1000);
    Services.prefs.setIntPref(PREF_BLOCKLIST_CLOCK_SKEW_SECONDS, clockDifference);
    Services.prefs.setIntPref(PREF_BLOCKLIST_LAST_UPDATE, serverTimeMillis / 1000);

    let firstError;
    for (let collectionInfo of versionInfo.data) {
      const {bucket, collection, last_modified: lastModified} = collectionInfo;
      const client = gBlocklistClients[collection];
      if (client && client.bucketName == bucket) {
        try {
          yield client.maybeSync(lastModified, serverTimeMillis);
        } catch (e) {
          if (!firstError) {
            firstError = e;
          }
        }
      }
    }
    if (firstError) {
      // cause the promise to reject by throwing the first observed error
      throw firstError;
    }

    // Save current Etag for next poll.
    if (response.headers.has("ETag")) {
      const currentEtag = response.headers.get("ETag");
      Services.prefs.setCharPref(PREF_BLOCKLIST_LAST_ETAG, currentEtag);
    }
  });
};
