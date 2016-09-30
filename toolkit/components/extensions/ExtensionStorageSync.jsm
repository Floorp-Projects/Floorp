/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO:
// * find out how the Chrome implementation deals with conflicts

"use strict";

/* exported extensionIdToCollectionId */

this.EXPORTED_SYMBOLS = ["ExtensionStorageSync"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;
const global = this;

Cu.import("resource://gre/modules/AppConstants.jsm");
const KINTO_PROD_SERVER_URL = "https://webextensions.settings.services.mozilla.com/v1";
const KINTO_DEV_SERVER_URL = "https://webextensions.dev.mozaws.net/v1";
const KINTO_DEFAULT_SERVER_URL = AppConstants.RELEASE_OR_BETA ? KINTO_PROD_SERVER_URL : KINTO_DEV_SERVER_URL;

const STORAGE_SYNC_ENABLED_PREF = "webextensions.storage.sync.enabled";
const STORAGE_SYNC_SERVER_URL_PREF = "webextensions.storage.sync.serverURL";
const STORAGE_SYNC_SCOPE = "sync:addon_storage";
const STORAGE_SYNC_CRYPTO_COLLECTION_NAME = "storage-sync-crypto";
const STORAGE_SYNC_CRYPTO_KEYRING_RECORD_ID = "keys";
const FXA_OAUTH_OPTIONS = {
  scope: STORAGE_SYNC_SCOPE,
};
// Default is 5sec, which seems a bit aggressive on the open internet
const KINTO_REQUEST_TIMEOUT = 30000;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
const {
  runSafeSyncWithoutClone,
} = Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppsUtils",
                                  "resource://gre/modules/AppsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CollectionKeyManager",
                                  "resource://services-sync/record.js");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "CryptoUtils",
                                  "resource://services-crypto/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "EncryptionRemoteTransformer",
                                  "resource://services-sync/engines/extension-storage.js");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorage",
                                  "resource://gre/modules/ExtensionStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "KintoHttpClient",
                                  "resource://services-common/kinto-http-client.js");
XPCOMUtils.defineLazyModuleGetter(this, "loadKinto",
                                  "resource://services-common/kinto-offline-client.js");
XPCOMUtils.defineLazyModuleGetter(this, "Log",
                                  "resource://gre/modules/Log.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Observers",
                                  "resource://services-common/observers.js");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "KeyRingEncryptionRemoteTransformer",
                                  "resource://services-sync/engines/extension-storage.js");
XPCOMUtils.defineLazyPreferenceGetter(this, "prefPermitsStorageSync",
                                      STORAGE_SYNC_ENABLED_PREF, false);
XPCOMUtils.defineLazyPreferenceGetter(this, "prefStorageSyncServerURL",
                                      STORAGE_SYNC_SERVER_URL_PREF,
                                      KINTO_DEFAULT_SERVER_URL);

/* globals prefPermitsStorageSync, prefStorageSyncServerURL */

// Map of Extensions to Set<Contexts> to track contexts that are still
// "live" and use storage.sync.
const extensionContexts = new Map();
// Borrow logger from Sync.
const log = Log.repository.getLogger("Sync.Engine.Extension-Storage");

/**
 * A Promise that centralizes initialization of ExtensionStorageSync.
 *
 * This centralizes the use of the Sqlite database, to which there is
 * only one connection which is shared by all threads.
 *
 * Fields in the object returned by this Promise:
 *
 * - connection: a Sqlite connection. Meant for internal use only.
 * - kinto: a KintoBase object, suitable for using in Firefox. All
 *   collections in this database will use the same Sqlite connection.
 */
const storageSyncInit = Task.spawn(function* () {
  const Kinto = loadKinto();
  const path = "storage-sync.sqlite";
  const opts = {path, sharedMemoryCache: false};
  const connection = yield Sqlite.openConnection(opts);
  yield Kinto.adapters.FirefoxAdapter._init(connection);
  return {
    connection,
    kinto: new Kinto({
      adapter: Kinto.adapters.FirefoxAdapter,
      adapterOptions: {sqliteHandle: connection},
      timeout: KINTO_REQUEST_TIMEOUT,
    }),
  };
});

AsyncShutdown.profileBeforeChange.addBlocker(
  "ExtensionStorageSync: close Sqlite handle",
  Task.async(function* () {
    const ret = yield storageSyncInit;
    const {connection} = ret;
    yield connection.close();
  })
);
// Kinto record IDs have two condtions:
//
// - They must contain only ASCII alphanumerics plus - and _. To fix
// this, we encode all non-letters using _C_, where C is the
// percent-encoded character, so space becomes _20_
// and underscore becomes _5F_.
//
// - They must start with an ASCII letter. To ensure this, we prefix
// all keys with "key-".
function keyToId(key) {
  function escapeChar(match) {
    return "_" + match.codePointAt(0).toString(16).toUpperCase() + "_";
  }
  return "key-" + key.replace(/[^a-zA-Z0-9]/g, escapeChar);
}

// Convert a Kinto ID back into a chrome.storage key.
// Returns null if a key couldn't be parsed.
function idToKey(id) {
  function unescapeNumber(match, group1) {
    return String.fromCodePoint(parseInt(group1, 16));
  }
  // An escaped ID should match this regex.
  // An escaped ID should consist of only letters and numbers, plus
  // code points escaped as _[0-9a-f]+_.
  const ESCAPED_ID_FORMAT = /^(?:[a-zA-Z0-9]|_[0-9A-F]+_)*$/;

  if (!id.startsWith("key-")) {
    return null;
  }
  const unprefixed = id.slice(4);
  // Verify that the ID is the correct format.
  if (!ESCAPED_ID_FORMAT.test(unprefixed)) {
    return null;
  }
  return unprefixed.replace(/_([0-9A-F]+)_/g, unescapeNumber);
}

// An "id schema" used to validate Kinto IDs and generate new ones.
const storageSyncIdSchema = {
  // We should never generate IDs; chrome.storage only acts as a
  // key-value store, so we should always have a key.
  generate() {
    throw new Error("cannot generate IDs");
  },

  // See keyToId and idToKey for more details.
  validate(id) {
    return idToKey(id) !== null;
  },
};

// An "id schema" used for the system collection, which doesn't
// require validation or generation of IDs.
const cryptoCollectionIdSchema = {
  generate() {
    throw new Error("cannot generate IDs for system collection");
  },

  validate(id) {
    return true;
  },
};

/**
 * Wrapper around the crypto collection providing some handy utilities.
 */
const cryptoCollection = this.cryptoCollection = {
  getCollection: Task.async(function* () {
    const {kinto} = yield storageSyncInit;
    return kinto.collection(STORAGE_SYNC_CRYPTO_COLLECTION_NAME, {
      idSchema: cryptoCollectionIdSchema,
      remoteTransformers: [new KeyRingEncryptionRemoteTransformer()],
    });
  }),

  /**
   * Retrieve the keyring record from the crypto collection.
   *
   * You can use this if you want to check metadata on the keyring
   * record rather than use the keyring itself.
   *
   * @returns {Promise<Object>}
   */
  getKeyRingRecord: Task.async(function* () {
    const collection = yield this.getCollection();
    const cryptoKeyRecord = yield collection.getAny(STORAGE_SYNC_CRYPTO_KEYRING_RECORD_ID);

    let data = cryptoKeyRecord.data;
    if (!data) {
      // This is a new keyring. Invent an ID for this record. If this
      // changes, it means a client replaced the keyring, so we need to
      // reupload everything.
      const uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
      const uuid = uuidgen.generateUUID();
      data = {uuid};
    }
    return data;
  }),

  /**
   * Retrieve the actual keyring from the crypto collection.
   *
   * @returns {Promise<CollectionKeyManager>}
   */
  getKeyRing: Task.async(function* () {
    const cryptoKeyRecord = yield this.getKeyRingRecord();
    const collectionKeys = new CollectionKeyManager();
    if (cryptoKeyRecord.keys) {
      collectionKeys.setContents(cryptoKeyRecord.keys, cryptoKeyRecord.last_modified);
    } else {
      // We never actually use the default key, so it's OK if we
      // generate one multiple times.
      collectionKeys.generateDefaultKey();
    }
    // Pass through uuid field so that we can save it if we need to.
    collectionKeys.uuid = cryptoKeyRecord.uuid;
    return collectionKeys;
  }),

  updateKBHash: Task.async(function* (kbHash) {
    const coll = yield this.getCollection();
    yield coll.update({id: STORAGE_SYNC_CRYPTO_KEYRING_RECORD_ID,
                       kbHash: kbHash},
                      {patch: true});
  }),

  upsert: Task.async(function* (record) {
    const collection = yield this.getCollection();
    yield collection.upsert(record);
  }),

  sync: Task.async(function* () {
    const collection = yield this.getCollection();
    return yield ExtensionStorageSync._syncCollection(collection, {
      strategy: "server_wins",
    });
  }),

  /**
   * Reset sync status for ALL collections by directly
   * accessing the FirefoxAdapter.
   */
  resetSyncStatus: Task.async(function* () {
    const coll = yield this.getCollection();
    yield coll.db.resetSyncStatus();
  }),

  // Used only for testing.
  _clear: Task.async(function* () {
    const collection = yield this.getCollection();
    yield collection.clear();
  }),
};

/**
 * An EncryptionRemoteTransformer that uses the special "keys" record
 * to find a key for a given extension.
 *
 * @param {string} extensionId The extension ID for which to find a key.
 */
class CollectionKeyEncryptionRemoteTransformer extends EncryptionRemoteTransformer {
  constructor(extensionId) {
    super();
    this.extensionId = extensionId;
  }

  getKeys() {
    const self = this;
    return Task.spawn(function* () {
      // FIXME: cache the crypto record for the duration of a sync cycle?
      const collectionKeys = yield cryptoCollection.getKeyRing();
      if (!collectionKeys.hasKeysFor([self.extensionId])) {
        // This should never happen. Keys should be created (and
        // synced) at the beginning of the sync cycle.
        throw new Error(`tried to encrypt records for ${this.extensionId}, but key is not present`);
      }
      return collectionKeys.keyForCollection(self.extensionId);
    });
  }
}
global.CollectionKeyEncryptionRemoteTransformer = CollectionKeyEncryptionRemoteTransformer;

/**
 * Clean up now that one context is no longer using this extension's collection.
 *
 * @param {Extension} extension
 *                    The extension whose context just ended.
 * @param {Context} context
 *                  The context that just ended.
 */
function cleanUpForContext(extension, context) {
  const contexts = extensionContexts.get(extension);
  if (!contexts) {
    Cu.reportError(new Error(`Internal error: cannot find any contexts for extension ${extension.id}`));
  }
  contexts.delete(context);
  if (contexts.size === 0) {
    // Nobody else is using this collection. Clean up.
    extensionContexts.delete(extension);
  }
}

/**
 * Generate a promise that produces the Collection for an extension.
 *
 * @param {Extension} extension
 *                    The extension whose collection needs to
 *                    be opened.
 * @param {Context} context
 *                  The context for this extension. The Collection
 *                  will shut down automatically when all contexts
 *                  close.
 * @returns {Promise<Collection>}
 */
const openCollection = Task.async(function* (extension, context) {
  let collectionId = extension.id;
  const {kinto} = yield storageSyncInit;
  const coll = kinto.collection(collectionId, {
    idSchema: storageSyncIdSchema,
    remoteTransformers: [new CollectionKeyEncryptionRemoteTransformer(extension.id)],
  });
  return coll;
});

/**
 * Hash an extension ID for a given user so that an attacker can't
 * identify the extensions a user has installed.
 *
 * @param {User} user
 *               The user for whom to choose a collection to sync
 *               an extension to.
 * @param {string} extensionId The extension ID to obfuscate.
 * @returns {string} A collection ID suitable for use to sync to.
 */
function extensionIdToCollectionId(user, extensionId) {
  const userFingerprint = CryptoUtils.hkdf(user.uid, undefined,
                                           "identity.mozilla.com/picl/v1/chrome.storage.sync.collectionIds", 2 * 32);
  let data = new TextEncoder().encode(userFingerprint + extensionId);
  let hasher = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  return CommonUtils.bytesAsHex(hasher.finish(false));
}

this.ExtensionStorageSync = {
  _fxaService: fxAccounts,
  listeners: new WeakMap(),

  syncAll: Task.async(function* () {
    const extensions = extensionContexts.keys();
    const extIds = Array.from(extensions, extension => extension.id);
    log.debug(`Syncing extension settings for ${JSON.stringify(extIds)}\n`);
    if (extIds.length == 0) {
      // No extensions to sync. Get out.
      return;
    }
    yield this.ensureKeysFor(extIds);
    yield this.checkSyncKeyRing();
    const promises = Array.from(extensionContexts.keys(), extension => {
      return openCollection(extension).then(coll => {
        return this.sync(extension, coll);
      });
    });
    yield Promise.all(promises);
  }),

  sync: Task.async(function* (extension, collection) {
    const signedInUser = yield this._fxaService.getSignedInUser();
    if (!signedInUser) {
      // FIXME: this should support syncing to self-hosted
      log.info("User was not signed into FxA; cannot sync");
      throw new Error("Not signed in to FxA");
    }
    const collectionId = extensionIdToCollectionId(signedInUser, extension.id);
    let syncResults;
    try {
      syncResults = yield this._syncCollection(collection, {
        strategy: "client_wins",
        collection: collectionId,
      });
    } catch (err) {
      log.warn("Syncing failed", err);
      throw err;
    }

    let changes = {};
    for (const record of syncResults.created) {
      changes[record.key] = {
        newValue: record.data,
      };
    }
    for (const record of syncResults.updated) {
      // N.B. It's safe to just pick old.key because it's not
      // possible to "rename" a record in the storage.sync API.
      const key = record.old.key;
      changes[key] = {
        oldValue: record.old.data,
        newValue: record.new.data,
      };
    }
    for (const record of syncResults.deleted) {
      changes[record.key] = {
        oldValue: record.data,
      };
    }
    for (const conflict of syncResults.resolved) {
      // FIXME: Should we even send a notification? If so, what
      // best values for "old" and "new"? This might violate
      // client code's assumptions, since from their perspective,
      // we were in state L, but this diff is from R -> L.
      changes[conflict.remote.key] = {
        oldValue: conflict.local.data,
        newValue: conflict.remote.data,
      };
    }
    if (Object.keys(changes).length > 0) {
      this.notifyListeners(extension, changes);
    }
  }),

  /**
   * Utility function that handles the common stuff about syncing all
   * Kinto collections (including "meta" collections like the crypto
   * one).
   *
   * @param {Collection} collection
   * @param {Object} options
   *                 Additional options to be passed to sync().
   * @returns {Promise<SyncResultObject>}
   */
  _syncCollection: Task.async(function* (collection, options) {
    // FIXME: this should support syncing to self-hosted
    return yield this._requestWithToken(`Syncing ${collection.name}`, function* (token) {
      const allOptions = Object.assign({}, {
        remote: prefStorageSyncServerURL,
        headers: {
          Authorization: "Bearer " + token,
        },
      }, options);

      return yield collection.sync(allOptions);
    });
  }),

  // Make a Kinto request with a current FxA token.
  // If the response indicates that the token might have expired,
  // retry the request.
  _requestWithToken: Task.async(function* (description, f) {
    const fxaToken = yield this._fxaService.getOAuthToken(FXA_OAUTH_OPTIONS);
    try {
      return yield f(fxaToken);
    } catch (e) {
      log.error(`${description}: request failed`, e);
      if (e && e.data && e.data.code == 401) {
        // Our token might have expired. Refresh and retry.
        log.info("Token might have expired");
        yield this._fxaService.removeCachedOAuthToken({token: fxaToken});
        const newToken = yield this._fxaService.getOAuthToken(FXA_OAUTH_OPTIONS);

        // If this fails too, let it go.
        return yield f(newToken);
      }
      // Otherwise, we don't know how to handle this error, so just reraise.
      throw e;
    }
  }),

  /**
   * Helper similar to _syncCollection, but for deleting the user's bucket.
   */
  _deleteBucket: Task.async(function* () {
    return yield this._requestWithToken("Clearing server", function* (token) {
      const headers = {Authorization: "Bearer " + token};
      const kintoHttp = new KintoHttpClient(prefStorageSyncServerURL, {
        headers: headers,
        timeout: KINTO_REQUEST_TIMEOUT,
      });
      return yield kintoHttp.deleteBucket("default");
    });
  }),

  /**
   * Recursive promise that terminates when our local collectionKeys,
   * as well as that on the server, have keys for all the extensions
   * in extIds.
   *
   * @param {Array<string>} extIds
   *                        The IDs of the extensions which need keys.
   * @returns {Promise<CollectionKeyManager>}
   */
  ensureKeysFor: Task.async(function* (extIds) {
    const collectionKeys = yield cryptoCollection.getKeyRing();
    if (collectionKeys.hasKeysFor(extIds)) {
      return collectionKeys;
    }

    const kbHash = yield this.getKBHash();
    const newKeys = yield collectionKeys.ensureKeysFor(extIds);
    const newRecord = {
      id: STORAGE_SYNC_CRYPTO_KEYRING_RECORD_ID,
      keys: newKeys.asWBO().cleartext,
      uuid: collectionKeys.uuid,
      // Add a field for the current kB hash.
      kbHash: kbHash,
    };
    yield cryptoCollection.upsert(newRecord);
    const result = yield this._syncKeyRing(newRecord);
    if (result.resolved.length != 0) {
      // We had a conflict which was automatically resolved. We now
      // have a new keyring which might have keys for the
      // collections. Recurse.
      return yield this.ensureKeysFor(extIds);
    }

    // No conflicts. We're good.
    return newKeys;
  }),

  /**
   * Get the current user's hashed kB.
   *
   * @returns sha256 of the user's kB as a hex string
   */
  getKBHash: Task.async(function* () {
    const signedInUser = yield this._fxaService.getSignedInUser();
    if (!signedInUser) {
      throw new Error("User isn't signed in!");
    }

    if (!signedInUser.kB) {
      throw new Error("User doesn't have kB??");
    }

    let kBbytes = CommonUtils.hexToBytes(signedInUser.kB);
    let hasher = Cc["@mozilla.org/security/hash;1"]
                    .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA256);
    return CommonUtils.bytesAsHex(CryptoUtils.digestBytes(signedInUser.uid + kBbytes, hasher));
  }),

  /**
   * Update the kB in the crypto record.
   */
  updateKeyRingKB: Task.async(function* () {
    const signedInUser = yield this._fxaService.getSignedInUser();
    if (!signedInUser) {
      // Although this function is meant to be called on login,
      // it's not unreasonable to check any time, even if we aren't
      // logged in.
      //
      // If we aren't logged in, we don't have any information about
      // the user's kB, so we can't be sure that the user changed
      // their kB, so just return.
      return;
    }

    const thisKBHash = yield this.getKBHash();
    yield cryptoCollection.updateKBHash(thisKBHash);
  }),

  /**
   * Make sure the keyring is up to date and synced.
   *
   * This is called on syncs to make sure that we don't sync anything
   * to any collection unless the key for that collection is on the
   * server.
   */
  checkSyncKeyRing: Task.async(function* () {
    yield this.updateKeyRingKB();

    const cryptoKeyRecord = yield cryptoCollection.getKeyRingRecord();
    if (cryptoKeyRecord && cryptoKeyRecord._status !== "synced") {
      // We haven't successfully synced the keyring since the last
      // change. This could be because kB changed and we touched the
      // keyring, or it could be because we failed to sync after
      // adding a key. Either way, take this opportunity to sync the
      // keyring.
      yield this._syncKeyRing(cryptoKeyRecord);
    }
  }),

  _syncKeyRing: Task.async(function* (cryptoKeyRecord) {
    try {
      // Try to sync using server_wins.
      //
      // We use server_wins here because whatever is on the server is
      // at least consistent with itself -- the crypto in the keyring
      // matches the crypto on the collection records. This is because
      // we generate and upload keys just before syncing data.
      //
      // It's possible that we can't decode the version on the server.
      // This can happen if a user is locked out of their account, and
      // does a "reset password" to get in on a new device. In this
      // case, we are in a bind -- we can't decrypt the record on the
      // server, so we can't merge keys. If this happens, we try to
      // figure out if we're the one with the correct (new) kB or if
      // we just got locked out because we have the old kB. If we're
      // the one with the correct kB, we wipe the server and reupload
      // everything, including a new keyring.
      //
      // If another device has wiped the server, we need to reupload
      // everything we have on our end too, so we detect this by
      // adding a UUID to the keyring. UUIDs are preserved throughout
      // the lifetime of a keyring, so the only time a keyring UUID
      // changes is when a new keyring is uploaded, which only happens
      // after a server wipe. So when we get a "conflict" (resolved by
      // server_wins), we check whether the server version has a new
      // UUID. If so, reset our sync status, so that we'll reupload
      // everything.
      const result = yield cryptoCollection.sync();
      if (result.resolved.length > 0) {
        if (result.resolved[0].uuid != cryptoKeyRecord.uuid) {
          log.info("Detected a new UUID. Reseting sync status for everything.");
          yield cryptoCollection.resetSyncStatus();

          // Server version is now correct. Return that result.
          return result;
        }
      }
      // No conflicts, or conflict was just someone else adding keys.
      return result;
    } catch (e) {
      if (KeyRingEncryptionRemoteTransformer.isOutdatedKB(e)) {
        // Check if our token is still valid, or if we got locked out
        // between starting the sync and talking to Kinto.
        const isSessionValid = yield this._fxaService.sessionStatus();
        if (isSessionValid) {
          yield this._deleteBucket();
          yield cryptoCollection.resetSyncStatus();

          // Reupload our keyring, which is the only new keyring.
          // We don't want client_wins here because another device
          // could have uploaded another keyring in the meantime.
          return yield cryptoCollection.sync();
        }
      }
      throw e;
    }
  }),

  /**
   * Get the collection for an extension, and register the extension
   * as being "in use".
   *
   * @param {Extension} extension
   *                    The extension for which we are seeking
   *                    a collection.
   * @param {Context} context
   *                  The context of the extension, so that we can
   *                  stop syncing the collection when the extension ends.
   * @returns {Promise<Collection>}
   */
  getCollection(extension, context) {
    if (prefPermitsStorageSync !== true) {
      return Promise.reject({message: `Please set ${STORAGE_SYNC_ENABLED_PREF} to true in about:config`});
    }
    // Register that the extension and context are in use.
    if (!extensionContexts.has(extension)) {
      extensionContexts.set(extension, new Set());
    }
    const contexts = extensionContexts.get(extension);
    if (!contexts.has(context)) {
      // New context. Register it and make sure it cleans itself up
      // when it closes.
      contexts.add(context);
      context.callOnClose({
        close: () => cleanUpForContext(extension, context),
      });
    }

    return openCollection(extension, context);
  },

  set: Task.async(function* (extension, items, context) {
    const coll = yield this.getCollection(extension, context);
    const keys = Object.keys(items);
    const ids = keys.map(keyToId);
    const changes = yield coll.execute(txn => {
      let changes = {};
      for (let [i, key] of keys.entries()) {
        const id = ids[i];
        let item = items[key];
        let {oldRecord} = txn.upsert({
          id,
          key,
          data: item,
        });
        changes[key] = {
          newValue: item,
        };
        if (oldRecord && oldRecord.data) {
          // Extract the "data" field from the old record, which
          // represents the value part of the key-value store
          changes[key].oldValue = oldRecord.data;
        }
      }
      return changes;
    }, {preloadIds: ids});
    this.notifyListeners(extension, changes);
  }),

  remove: Task.async(function* (extension, keys, context) {
    const coll = yield this.getCollection(extension, context);
    keys = [].concat(keys);
    const ids = keys.map(keyToId);
    let changes = {};
    yield coll.execute(txn => {
      for (let [i, key] of keys.entries()) {
        const id = ids[i];
        const res = txn.deleteAny(id);
        if (res.deleted) {
          changes[key] = {
            oldValue: res.data.data,
          };
        }
      }
      return changes;
    }, {preloadIds: ids});
    if (Object.keys(changes).length > 0) {
      this.notifyListeners(extension, changes);
    }
  }),

  clear: Task.async(function* (extension, context) {
    // We can't call Collection#clear here, because that just clears
    // the local database. We have to explicitly delete everything so
    // that the deletions can be synced as well.
    const coll = yield this.getCollection(extension, context);
    const res = yield coll.list();
    const records = res.data;
    const keys = records.map(record => record.key);
    yield this.remove(extension, keys, context);
  }),

  get: Task.async(function* (extension, spec, context) {
    const coll = yield this.getCollection(extension, context);
    let keys, records;
    if (spec === null) {
      records = {};
      const res = yield coll.list();
      for (let record of res.data) {
        records[record.key] = record.data;
      }
      return records;
    }
    if (typeof spec === "string") {
      keys = [spec];
      records = {};
    } else if (Array.isArray(spec)) {
      keys = spec;
      records = {};
    } else {
      keys = Object.keys(spec);
      records = Cu.cloneInto(spec, global);
    }

    for (let key of keys) {
      const res = yield coll.getAny(keyToId(key));
      if (res.data && res.data._status != "deleted") {
        records[res.data.key] = res.data.data;
      }
    }

    return records;
  }),

  addOnChangedListener(extension, listener, context) {
    let listeners = this.listeners.get(extension) || new Set();
    listeners.add(listener);
    this.listeners.set(extension, listeners);

    // Force opening the collection so that we will sync for this extension.
    return this.getCollection(extension, context);
  },

  removeOnChangedListener(extension, listener) {
    let listeners = this.listeners.get(extension);
    listeners.delete(listener);
    if (listeners.size == 0) {
      this.listeners.delete(extension);
    }
  },

  notifyListeners(extension, changes) {
    Observers.notify("ext.storage.sync-changed");
    let listeners = this.listeners.get(extension) || new Set();
    if (listeners) {
      for (let listener of listeners) {
        runSafeSyncWithoutClone(listener, changes);
      }
    }
  },
};
