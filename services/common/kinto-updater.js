/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["checkVersions", "addTestKintoClient"];

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.importGlobalProperties(['fetch']);
const BlocklistClients = Cu.import("resource://services-common/KintoBlocklist.js", {});

const PREF_KINTO_CHANGES_PATH       = "services.kinto.changes.path";
const PREF_KINTO_BASE               = "services.kinto.base";
const PREF_KINTO_BUCKET             = "services.kinto.bucket";
const PREF_KINTO_LAST_UPDATE        = "services.kinto.last_update_seconds";
const PREF_KINTO_LAST_ETAG          = "services.kinto.last_etag";
const PREF_KINTO_CLOCK_SKEW_SECONDS = "services.kinto.clock_skew_seconds";
const PREF_KINTO_ONECRL_COLLECTION  = "services.kinto.onecrl.collection";


const gBlocklistClients = {
  [BlocklistClients.OneCRLBlocklistClient.collectionName]: BlocklistClients.OneCRLBlocklistClient,
  [BlocklistClients.AddonBlocklistClient.collectionName]: BlocklistClients.AddonBlocklistClient,
  [BlocklistClients.GfxBlocklistClient.collectionName]: BlocklistClients.GfxBlocklistClient,
  [BlocklistClients.PluginBlocklistClient.collectionName]: BlocklistClients.PluginBlocklistClient
};

// Add a blocklist client for testing purposes. Do not use for any other purpose
this.addTestKintoClient = (name, client) => { gBlocklistClients[name] = client; }

// This is called by the ping mechanism.
// returns a promise that rejects if something goes wrong
this.checkVersions = function() {
  return Task.spawn(function* syncClients() {
    // Fetch a versionInfo object that looks like:
    // {"data":[
    //   {
    //     "host":"kinto-ota.dev.mozaws.net",
    //     "last_modified":1450717104423,
    //     "bucket":"blocklists",
    //     "collection":"certificates"
    //    }]}
    // Right now, we only use the collection name and the last modified info
    let kintoBase = Services.prefs.getCharPref(PREF_KINTO_BASE);
    let changesEndpoint = kintoBase + Services.prefs.getCharPref(PREF_KINTO_CHANGES_PATH);
    let blocklistsBucket = Services.prefs.getCharPref(PREF_KINTO_BUCKET);

    // Use ETag to obtain a `304 Not modified` when no change occurred.
    const headers = {};
    if (Services.prefs.prefHasUserValue(PREF_KINTO_LAST_ETAG)) {
      const lastEtag = Services.prefs.getCharPref(PREF_KINTO_LAST_ETAG);
      if (lastEtag) {
        headers["If-None-Match"] = lastEtag;
      }
    }

    let response = yield fetch(changesEndpoint, {headers});

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
    let serverTimeMillis = Date.parse(response.headers.get("Date"));

    // negative clockDifference means local time is behind server time
    // by the absolute of that value in seconds (positive means it's ahead)
    let clockDifference = Math.floor((Date.now() - serverTimeMillis) / 1000);
    Services.prefs.setIntPref(PREF_KINTO_CLOCK_SKEW_SECONDS, clockDifference);
    Services.prefs.setIntPref(PREF_KINTO_LAST_UPDATE, serverTimeMillis / 1000);

    let firstError;
    for (let collectionInfo of versionInfo.data) {
      // Skip changes that don't concern configured blocklist bucket.
      if (collectionInfo.bucket != blocklistsBucket) {
        continue;
      }

      let collection = collectionInfo.collection;
      let client = gBlocklistClients[collection];
      if (client && client.maybeSync) {
        let lastModified = 0;
        if (collectionInfo.last_modified) {
          lastModified = collectionInfo.last_modified;
        }
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
      Services.prefs.setCharPref(PREF_KINTO_LAST_ETAG, currentEtag);
    }
  });
};
