/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AddonBlocklistClient",
                         "GfxBlocklistClient",
                         "OneCRLBlocklistClient",
                         "PluginBlocklistClient",
                         "FILENAME_ADDONS_JSON",
                         "FILENAME_GFX_JSON",
                         "FILENAME_PLUGINS_JSON"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
const { Task } = Cu.import("resource://gre/modules/Task.jsm");
const { OS } = Cu.import("resource://gre/modules/osfile.jsm");

const { loadKinto } = Cu.import("resource://services-common/kinto-offline-client.js");

const PREF_KINTO_BASE                    = "services.kinto.base";
const PREF_KINTO_BUCKET                  = "services.kinto.bucket";
const PREF_KINTO_ONECRL_COLLECTION       = "services.kinto.onecrl.collection";
const PREF_KINTO_ONECRL_CHECKED_SECONDS  = "services.kinto.onecrl.checked";
const PREF_KINTO_ADDONS_COLLECTION       = "services.kinto.addons.collection";
const PREF_KINTO_ADDONS_CHECKED_SECONDS  = "services.kinto.addons.checked";
const PREF_KINTO_PLUGINS_COLLECTION      = "services.kinto.plugins.collection";
const PREF_KINTO_PLUGINS_CHECKED_SECONDS = "services.kinto.plugins.checked";
const PREF_KINTO_GFX_COLLECTION          = "services.kinto.gfx.collection";
const PREF_KINTO_GFX_CHECKED_SECONDS     = "services.kinto.gfx.checked";

this.FILENAME_ADDONS_JSON  = "blocklist-addons.json";
this.FILENAME_GFX_JSON     = "blocklist-gfx.json";
this.FILENAME_PLUGINS_JSON = "blocklist-plugins.json";


/**
 * Helper to instantiate a Kinto client based on preferences for remote server
 * URL and bucket name. It uses the `FirefoxAdapter` which relies on SQLite to
 * persist the local DB.
 */
function kintoClient() {
  let base = Services.prefs.getCharPref(PREF_KINTO_BASE);
  let bucket = Services.prefs.getCharPref(PREF_KINTO_BUCKET);

  let Kinto = loadKinto();

  let FirefoxAdapter = Kinto.adapters.FirefoxAdapter;

  let config = {
    remote: base,
    bucket: bucket,
    adapter: FirefoxAdapter,
  };

  return new Kinto(config);
}


class BlocklistClient {

  constructor(collectionName, lastCheckTimePref, processCallback) {
    this.collectionName = collectionName;
    this.lastCheckTimePref = lastCheckTimePref;
    this.processCallback = processCallback;
  }

  /**
   * Synchronize from Kinto server, if necessary.
   *
   * @param {int}  lastModified the lastModified date (on the server) for
                                the remote collection.
   * @param {Date} serverTime   the current date return by the server.
   * @return {Promise}          which rejects on sync or process failure.
   */
  maybeSync(lastModified, serverTime) {
    let db = kintoClient();
    let collection = db.collection(this.collectionName);

    return Task.spawn((function* syncCollection() {
      try {
        yield collection.db.open();

        let collectionLastModified = yield collection.db.getLastModified();
        // If the data is up to date, there's no need to sync. We still need
        // to record the fact that a check happened.
        if (lastModified <= collectionLastModified) {
          this.updateLastCheck(serverTime);
          return;
        }
        // Fetch changes from server.
        yield collection.sync();
        // Read local collection of records.
        let list = yield collection.list();

        yield this.processCallback(list.data);

        // Track last update.
        this.updateLastCheck(serverTime);
      } finally {
        collection.db.close();
      }
    }).bind(this));
  }

  /**
   * Save last time server was checked in users prefs.
   *
   * @param {Date} serverTime   the current date return by server.
   */
  updateLastCheck(serverTime) {
    let checkedServerTimeInSeconds = Math.round(serverTime / 1000);
    Services.prefs.setIntPref(this.lastCheckTimePref, checkedServerTimeInSeconds);
  }
}

/**
 * Revoke the appropriate certificates based on the records from the blocklist.
 *
 * @param {Object} records   current records in the local db.
 */
function* updateCertBlocklist(records) {
  let certList = Cc["@mozilla.org/security/certblocklist;1"]
                   .getService(Ci.nsICertBlocklist);
  for (let item of records) {
    if (item.issuerName && item.serialNumber) {
      certList.revokeCertByIssuerAndSerial(item.issuerName,
                                           item.serialNumber);
    } else if (item.subject && item.pubKeyHash) {
      certList.revokeCertBySubjectAndPubKey(item.subject,
                                            item.pubKeyHash);
    } else {
      throw new Error("Cert blocklist record has incomplete data");
    }
  }
  certList.saveEntries();
}

/**
 * Write list of records into JSON file, and notify nsBlocklistService.
 *
 * @param {String} filename  path relative to profile dir.
 * @param {Object} records   current records in the local db.
 */
function* updateJSONBlocklist(filename, records) {
  // Write JSON dump for synchronous load at startup.
  const path = OS.Path.join(OS.Constants.Path.profileDir, filename);
  const serialized = JSON.stringify({data: records}, null, 2);
  try {
    yield OS.File.writeAtomic(path, serialized, {tmpPath: path + ".tmp"});

    // Notify change to `nsBlocklistService`
    const eventData = {filename: filename};
    Services.cpmm.sendAsyncMessage("Blocklist:reload-from-disk", eventData);
  } catch(e) {
    Cu.reportError(e);
  }
}


this.OneCRLBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_KINTO_ONECRL_COLLECTION),
  PREF_KINTO_ONECRL_CHECKED_SECONDS,
  updateCertBlocklist
);

this.AddonBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_KINTO_ADDONS_COLLECTION),
  PREF_KINTO_ADDONS_CHECKED_SECONDS,
  updateJSONBlocklist.bind(undefined, FILENAME_ADDONS_JSON)
);

this.GfxBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_KINTO_GFX_COLLECTION),
  PREF_KINTO_GFX_CHECKED_SECONDS,
  updateJSONBlocklist.bind(undefined, FILENAME_GFX_JSON)
);

this.PluginBlocklistClient = new BlocklistClient(
  Services.prefs.getCharPref(PREF_KINTO_PLUGINS_COLLECTION),
  PREF_KINTO_PLUGINS_CHECKED_SECONDS,
  updateJSONBlocklist.bind(undefined, FILENAME_PLUGINS_JSON)
);
