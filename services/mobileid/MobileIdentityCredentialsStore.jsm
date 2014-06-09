/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MobileIdentityCredentialsStore"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/MobileIdentityCommon.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const CREDENTIALS_DB_NAME     = "mobile-id-credentials";
const CREDENTIALS_DB_VERSION  = 1;
const CREDENTIALS_STORE_NAME  = "credentials-store";

this.MobileIdentityCredentialsStore = function() {
};

this.MobileIdentityCredentialsStore.prototype = {

  __proto__: IndexedDBHelper.prototype,

  init: function() {
    log.debug("MobileIdentityCredentialsStore init");
    this.initDBHelper(CREDENTIALS_DB_NAME,
                      CREDENTIALS_DB_VERSION,
                      [CREDENTIALS_STORE_NAME]);
  },

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    log.debug("upgradeSchema");
    /**
     * We will be storing objects like:
     *  {
     *    msisdn: <string> (key),
     *    iccId: <string> (index),
     *    origin: <array> (index),
     *    msisdnSessionToken: <string>
     *  }
     */
    let objectStore = aDb.createObjectStore(CREDENTIALS_STORE_NAME, {
      keyPath: "msisdn"
    });

    objectStore.createIndex("iccId", "iccId", { unique: true });
    objectStore.createIndex("origin", "origin", { unique: true, multiEntry: true });
  },

  add: function(aIccId, aMsisdn, aOrigin, aSessionToken) {
    log.debug("put " + aIccId + ", " + aMsisdn + ", " + aOrigin + ", " +
              aSessionToken);
    if (!aOrigin || !aSessionToken) {
      return Promise.reject(ERROR_INTERNAL_DB_ERROR);
    }

    let deferred = Promise.defer();

    // We first try get an existing record for the given MSISDN.
    this.newTxn(
      "readwrite",
      CREDENTIALS_STORE_NAME,
      (aTxn, aStore) => {
        let range = IDBKeyRange.only(aMsisdn);
        let cursorReq = aStore.openCursor(range);
        cursorReq.onsuccess = function(aEvent) {
          let cursor = aEvent.target.result;
          let record;
          // If we already have a record of this MSISDN, we add the origin to
          // the list of allowed origins.
          if (cursor && cursor.value) {
            record = cursor.value;
            if (record.origin.indexOf(aOrigin) == -1) {
              record.origin.push(aOrigin);
            }
            cursor.update(record);
          } else {
            // Otherwise, we store a new record.
            record = {
              iccId: aIccId,
              msisdn: aMsisdn,
              origin: [aOrigin],
              sessionToken: aSessionToken
            };
            aStore.add(record);
          }
          deferred.resolve();
        };
        cursorReq.onerror = function(aEvent) {
          log.error(aEvent.target.error);
          deferred.reject(ERROR_INTERNAL_DB_ERROR);
        };
      }, null, deferred.reject);

    return deferred.promise;
  },

  getByMsisdn: function(aMsisdn) {
    log.debug("getByMsisdn " + aMsisdn);
    if (!aMsisdn) {
      return Promise.resolve(null);
    }

    let deferred = Promise.defer();
    this.newTxn(
      "readonly",
      CREDENTIALS_STORE_NAME,
      (aTxn, aStore) => {
        aStore.get(aMsisdn).onsuccess = function(aEvent) {
          aTxn.result = aEvent.target.result;
        };
      },
      function(result) {
        deferred.resolve(result);
      },
      deferred.reject
    );
    return deferred.promise;
  },

  getByIndex: function(aIndex, aValue) {
    log.debug("getByIndex " + aIndex + ", " + aValue);
    if (!aValue || !aIndex) {
      return Promise.resolve(null);
    }

    let deferred = Promise.defer();
    this.newTxn(
      "readonly",
      CREDENTIALS_STORE_NAME,
      (aTxn, aStore) => {
        let index = aStore.index(aIndex);
        index.get(aValue).onsuccess = function(aEvent) {
          aTxn.result = aEvent.target.result;
        };
      },
      function(result) {
        deferred.resolve(result);
      },
      deferred.reject
    );
    return deferred.promise;
  },

  getByOrigin: function(aOrigin) {
    return this.getByIndex("origin", aOrigin);
  },

  getByIccId: function(aIccId) {
    return this.getByIndex("iccId", aIccId);
  },

  delete: function(aMsisdn) {
    log.debug("delete " + aMsisdn);
    if (!aMsisdn) {
      return Promise.resolve();
    }

    let deferred = Promise.defer();
    this.newTxn(
      "readwrite",
      CREDENTIALS_STORE_NAME,
      (aTxn, aStore) => {
        aStore.delete(aMsisdn);
      },
      deferred.resolve,
      deferred.reject
    );
    return deferred.promise;
  }
};
