/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["OneCRLClient"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://services-common/moz-kinto-client.js");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

const PREF_KINTO_BASE = "services.kinto.base";
const PREF_KINTO_BUCKET = "services.kinto.bucket";
const PREF_KINTO_ONECRL_COLLECTION = "services.kinto.onecrl.collection";
const PREF_KINTO_ONECRL_CHECKED_SECONDS = "services.kinto.onecrl.checked";

const RE_UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

// Kinto.js assumes version 4 UUIDs but allows you to specify custom
// validators and generators. The tooling that generates records in the
// certificates collection currently uses a version 1 UUID so we must
// specify a validator that's less strict. We must also supply a generator
// since Kinto.js does not allow one without the other.
function makeIDSchema() {
  return {
    validate: RE_UUID.test.bind(RE_UUID),
    generate: function() {
      return uuidgen.generateUUID().toString();
    }
  };
}

// A Kinto based client to keep the OneCRL certificate blocklist up to date.
function CertBlocklistClient() {
  // maybe sync the collection of certificates with remote data.
  // lastModified - the lastModified date (on the server, milliseconds since
  // epoch) of data in the remote collection
  // serverTime - the time on the server (milliseconds since epoch)
  // returns a promise which rejects on sync failure
  this.maybeSync = function(lastModified, serverTime) {
    let base = Services.prefs.getCharPref(PREF_KINTO_BASE);
    let bucket = Services.prefs.getCharPref(PREF_KINTO_BUCKET);

    let Kinto = loadKinto();

    let FirefoxAdapter = Kinto.adapters.FirefoxAdapter;


    let certList = Cc["@mozilla.org/security/certblocklist;1"]
                     .getService(Ci.nsICertBlocklist);

    // Future blocklist clients can extract the sync-if-stale logic. For
    // now, since this is currently the only client, we'll do this here.
    let config = {
      remote: base,
      bucket: bucket,
      adapter: FirefoxAdapter,
    };

    let db = new Kinto(config);
    let collectionName = Services.prefs.getCharPref(PREF_KINTO_ONECRL_COLLECTION,
                                                    "certificates");
    let blocklist = db.collection(collectionName,
                                  { idSchema: makeIDSchema() });

    let updateLastCheck = function() {
      let checkedServerTimeInSeconds = Math.round(serverTime / 1000);
      Services.prefs.setIntPref(PREF_KINTO_ONECRL_CHECKED_SECONDS,
                                checkedServerTimeInSeconds);
    }

    return Task.spawn(function* () {
      try {
        yield blocklist.db.open();
        let collectionLastModified = yield blocklist.db.getLastModified();
        // if the data is up to date, there's no need to sync. We still need
        // to record the fact that a check happened.
        if (lastModified <= collectionLastModified) {
          updateLastCheck();
          return;
        }
        yield blocklist.sync();
        let list = yield blocklist.list();
        for (let item of list.data) {
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
        // We explicitly do not want to save entries or update the
        // last-checked time if sync fails
        certList.saveEntries();
        updateLastCheck();
      } finally {
        blocklist.db.close()
      }
    });
  }
}

this.OneCRLClient = new CertBlocklistClient();
