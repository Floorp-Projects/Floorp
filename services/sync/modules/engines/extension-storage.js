/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ['ExtensionStorageEngine', 'EncryptionRemoteTransformer',
                         'KeyRingEncryptionRemoteTransformer'];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/keys.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/async.js");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionStorageSync",
                                  "resource://gre/modules/ExtensionStorageSync.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * The Engine that manages syncing for the web extension "storage"
 * API, and in particular ext.storage.sync.
 *
 * ext.storage.sync is implemented using Kinto, so it has mechanisms
 * for syncing that we do not need to integrate in the Firefox Sync
 * framework, so this is something of a stub.
 */
this.ExtensionStorageEngine = function ExtensionStorageEngine(service) {
  SyncEngine.call(this, "Extension-Storage", service);
};
ExtensionStorageEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _trackerObj: ExtensionStorageTracker,
  // we don't need these since we implement our own sync logic
  _storeObj: undefined,
  _recordObj: undefined,

  syncPriority: 10,
  allowSkippedRecord: false,

  _sync: function () {
    return Async.promiseSpinningly(ExtensionStorageSync.syncAll());
  },

  get enabled() {
    // By default, we sync extension storage if we sync addons. This
    // lets us simplify the UX since users probably don't consider
    // "extension preferences" a separate category of syncing.
    // However, we also respect engine.extension-storage.force, which
    // can be set to true or false, if a power user wants to customize
    // the behavior despite the lack of UI.
    const forced = Svc.Prefs.get("engine." + this.prefName + ".force", undefined);
    if (forced !== undefined) {
      return forced;
    }
    return Svc.Prefs.get("engine.addons", false);
  },
};

function ExtensionStorageTracker(name, engine) {
  Tracker.call(this, name, engine);
}
ExtensionStorageTracker.prototype = {
  __proto__: Tracker.prototype,

  startTracking: function () {
    Svc.Obs.add("ext.storage.sync-changed", this);
  },

  stopTracking: function () {
    Svc.Obs.remove("ext.storage.sync-changed", this);
  },

  observe: function (subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    if (this.ignoreAll) {
      return;
    }

    if (topic !== "ext.storage.sync-changed") {
      return;
    }

    // Single adds, removes and changes are not so important on their
    // own, so let's just increment score a bit.
    this.score += SCORE_INCREMENT_MEDIUM;
  },

  // Override a bunch of methods which don't do anything for us.
  // This is a performance hack.
  ignoreID: function() {
  },
  unignoreID: function() {
  },
  addChangedID: function() {
  },
  removeChangedID: function() {
  },
  clearChangedIDs: function() {
  },
};

/**
 * Utility function to enforce an order of fields when computing an HMAC.
 */
function ciphertextHMAC(keyBundle, id, IV, ciphertext) {
  const hasher = keyBundle.sha256HMACHasher;
  return Utils.bytesAsHex(Utils.digestUTF8(id + IV + ciphertext, hasher));
}

/**
 * A "remote transformer" that the Kinto library will use to
 * encrypt/decrypt records when syncing.
 *
 * This is an "abstract base class". Subclass this and override
 * getKeys() to use it.
 */
class EncryptionRemoteTransformer {
  encode(record) {
    const self = this;
    return Task.spawn(function* () {
      const keyBundle = yield self.getKeys();
      if (record.ciphertext) {
        throw new Error("Attempt to reencrypt??");
      }
      let id = record.id;
      if (!record.id) {
        throw new Error("Record ID is missing or invalid");
      }

      let IV = Svc.Crypto.generateRandomIV();
      let ciphertext = Svc.Crypto.encrypt(JSON.stringify(record),
                                          keyBundle.encryptionKeyB64, IV);
      let hmac = ciphertextHMAC(keyBundle, id, IV, ciphertext);
      const encryptedResult = {ciphertext, IV, hmac, id};
      if (record.hasOwnProperty("last_modified")) {
        encryptedResult.last_modified = record.last_modified;
      }
      return encryptedResult;
    });
  }

  decode(record) {
    const self = this;
    return Task.spawn(function* () {
      const keyBundle = yield self.getKeys();
      if (!record.ciphertext) {
        throw new Error("No ciphertext: nothing to decrypt?");
      }
      // Authenticate the encrypted blob with the expected HMAC
      let computedHMAC = ciphertextHMAC(keyBundle, record.id, record.IV, record.ciphertext);

      if (computedHMAC != record.hmac) {
        Utils.throwHMACMismatch(record.hmac, computedHMAC);
      }

      // Handle invalid data here. Elsewhere we assume that cleartext is an object.
      let cleartext = Svc.Crypto.decrypt(record.ciphertext,
                                         keyBundle.encryptionKeyB64, record.IV);
      let jsonResult = JSON.parse(cleartext);
      if (!jsonResult || typeof jsonResult !== "object") {
        throw new Error("Decryption failed: result is <" + jsonResult + ">, not an object.");
      }

      // Verify that the encrypted id matches the requested record's id.
      // This should always be true, because we compute the HMAC over
      // the original record's ID, and that was verified already (above).
      if (jsonResult.id != record.id) {
        throw new Error("Record id mismatch: " + jsonResult.id + " != " + record.id);
      }

      if (record.hasOwnProperty("last_modified")) {
        jsonResult.last_modified = record.last_modified;
      }

      return jsonResult;
    });
  }

  /**
   * Retrieve keys to use during encryption.
   *
   * Returns a Promise<KeyBundle>.
   */
  getKeys() {
    throw new Error("override getKeys in a subclass");
  }
}
// You can inject this
EncryptionRemoteTransformer.prototype._fxaService = fxAccounts;

/**
 * An EncryptionRemoteTransformer that provides a keybundle derived
 * from the user's kB, suitable for encrypting a keyring.
 */
class KeyRingEncryptionRemoteTransformer extends EncryptionRemoteTransformer {
  getKeys() {
    const self = this;
    return Task.spawn(function* () {
      const user = yield self._fxaService.getSignedInUser();
      // FIXME: we should permit this if the user is self-hosting
      // their storage
      if (!user) {
        throw new Error("user isn't signed in to FxA; can't sync");
      }

      if (!user.kB) {
        throw new Error("user doesn't have kB");
      }

      let kB = Utils.hexToBytes(user.kB);

      let keyMaterial = CryptoUtils.hkdf(kB, undefined,
                                       "identity.mozilla.com/picl/v1/chrome.storage.sync", 2*32);
      let bundle = new BulkKeyBundle();
      // [encryptionKey, hmacKey]
      bundle.keyPair = [keyMaterial.slice(0, 32), keyMaterial.slice(32, 64)];
      return bundle;
    });
  }
  // Pass through the kbHash field from the unencrypted record. If
  // encryption fails, we can use this to try to detect whether we are
  // being compromised or if the record here was encoded with a
  // different kB.
  encode(record) {
    const encodePromise = super.encode(record);
    return Task.spawn(function* () {
      const encoded = yield encodePromise;
      encoded.kbHash = record.kbHash;
      return encoded;
    });
  }

  decode(record) {
    const decodePromise = super.decode(record);
    return Task.spawn(function* () {
      try {
        return yield decodePromise;
      } catch (e) {
        if (Utils.isHMACMismatch(e)) {
          const currentKBHash = yield ExtensionStorageSync.getKBHash();
          if (record.kbHash != currentKBHash) {
            // Some other client encoded this with a kB that we don't
            // have access to.
            KeyRingEncryptionRemoteTransformer.throwOutdatedKB(currentKBHash, record.kbHash);
          }
        }
        throw e;
      }
    });
  }

  // Generator and discriminator for KB-is-outdated exceptions.
  static throwOutdatedKB(shouldBe, is) {
    throw new Error(`kB hash on record is outdated: should be ${shouldBe}, is ${is}`);
  }

  static isOutdatedKB(exc) {
    const kbMessage = "kB hash on record is outdated: ";
    return exc && exc.message && exc.message.indexOf &&
      (exc.message.indexOf(kbMessage) == 0);
  }
}
