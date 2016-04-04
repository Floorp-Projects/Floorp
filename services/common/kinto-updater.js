/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["checkVersions", "addTestKintoClient"];

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.importGlobalProperties(['fetch']);

const PREF_KINTO_CHANGES_PATH = "services.kinto.changes.path";
const PREF_KINTO_BASE = "services.kinto.base";
const PREF_KINTO_LAST_UPDATE = "services.kinto.last_update_seconds";
const PREF_KINTO_CLOCK_SKEW_SECONDS = "services.kinto.clock_skew_seconds";

const kintoClients = {
};

// This is called by the ping mechanism.
// returns a promise that rejects if something goes wrong
this.checkVersions = function() {
  return Task.spawn(function *() {
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

    let response = yield fetch(changesEndpoint);

    // Record new update time and the difference between local and server time
    let serverTimeMillis = Date.parse(response.headers.get("Date"));
    let clockDifference = Math.abs(Date.now() - serverTimeMillis) / 1000;
    Services.prefs.setIntPref(PREF_KINTO_LAST_UPDATE, serverTimeMillis / 1000);
    Services.prefs.setIntPref(PREF_KINTO_CLOCK_SKEW_SECONDS, clockDifference);

    let versionInfo = yield response.json();

    let firstError;
    for (let collectionInfo of versionInfo.data) {
      let collection = collectionInfo.collection;
      let kintoClient = kintoClients[collection];
      if (kintoClient && kintoClient.maybeSync) {
        let lastModified = 0;
        if (collectionInfo.last_modified) {
          lastModified = collectionInfo.last_modified
        }
        try {
          yield kintoClient.maybeSync(lastModified, serverTimeMillis);
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
  });
};

// Add a kintoClient for testing purposes. Do not use for any other purpose
this.addTestKintoClient = function(name, kintoClient) {
  kintoClients[name] = kintoClient;
};

// Add the various things that we know want updates
kintoClients.certificates =
  Cu.import("resource://services-common/KintoCertificateBlocklist.js", {})
  .OneCRLClient;
