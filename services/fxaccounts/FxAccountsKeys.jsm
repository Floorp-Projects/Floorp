/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

const { CryptoUtils } = ChromeUtils.import(
  "resource://services-crypto/utils.js"
);

const { DERIVED_KEYS_NAMES, SCOPE_OLD_SYNC, log, logPII } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
);

class FxAccountsKeys {
  constructor(fxAccountsInternal) {
    this._fxia = fxAccountsInternal;
  }

  /**
   * Checks if we currently have encryption keys or if we have enough to
   * be able to successfully fetch them for the signed-in-user.
   */
  canGetKeys() {
    return this._fxia.withCurrentAccountState(async currentState => {
      let userData = await currentState.getUserAccountData();
      if (!userData) {
        throw new Error("Can't possibly get keys; User is not signed in");
      }
      // - keyFetchToken means we can almost certainly grab them.
      // - kSync, kXCS, kExtSync and kExtKbHash means we already have them.
      // - kB is deprecated but |getKeys| will help us migrate to kSync and friends.
      return (
        userData &&
        (userData.keyFetchToken ||
          DERIVED_KEYS_NAMES.every(k => userData[k]) ||
          userData.kB)
      );
    });
  }

  /**
   * Fetch encryption keys for the signed-in-user from the FxA API server.
   *
   * Not for user consumption.  Exists to cause the keys to be fetch.
   *
   * Returns user data so that it can be chained with other methods.
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          kSync: An encryption key for Sync
   *          kXCS: A key hash of kB for the X-Client-State header
   *          kExtSync: An encryption key for WebExtensions syncing
   *          kExtKbHash: A key hash of kB for WebExtensions syncing
   *          verified: email verification status
   *        }
   *        or null if no user is signed in
   */
  async getKeys() {
    return this._fxia.withCurrentAccountState(async currentState => {
      try {
        let userData = await currentState.getUserAccountData();
        if (!userData) {
          throw new Error("Can't get keys; User is not signed in");
        }
        if (userData.kB) {
          // Bug 1426306 - Migrate from kB to derived keys.
          log.info("Migrating kB to derived keys.");
          const { uid, kB } = userData;
          await currentState.updateUserAccountData({
            uid,
            ...(await this._deriveKeys(uid, CommonUtils.hexToBytes(kB))),
            kA: null, // Remove kA and kB from storage.
            kB: null,
          });
          userData = await currentState.getUserAccountData();
        }
        if (DERIVED_KEYS_NAMES.every(k => userData[k])) {
          return userData;
        }
        if (!currentState.whenKeysReadyDeferred) {
          currentState.whenKeysReadyDeferred = PromiseUtils.defer();
          if (userData.keyFetchToken) {
            this.fetchAndUnwrapKeys(userData.keyFetchToken).then(
              dataWithKeys => {
                if (DERIVED_KEYS_NAMES.some(k => !dataWithKeys[k])) {
                  const missing = DERIVED_KEYS_NAMES.filter(
                    k => !dataWithKeys[k]
                  );
                  currentState.whenKeysReadyDeferred.reject(
                    new Error(`user data missing: ${missing.join(", ")}`)
                  );
                  return;
                }
                currentState.whenKeysReadyDeferred.resolve(dataWithKeys);
              },
              err => {
                currentState.whenKeysReadyDeferred.reject(err);
              }
            );
          } else {
            currentState.whenKeysReadyDeferred.reject("No keyFetchToken");
          }
        }
        return await currentState.whenKeysReadyDeferred.promise;
      } catch (err) {
        return this._fxia._handleTokenError(err);
      }
    });
  }

  /**
   * Once the user's email is verified, we can request the keys
   */
  fetchKeys(keyFetchToken) {
    let client = this._fxia.fxAccountsClient;
    log.debug(
      `Fetching keys with token ${!!keyFetchToken} from ${client.host}`
    );
    if (logPII) {
      log.debug("fetchKeys - the token is " + keyFetchToken);
    }
    return client.accountKeys(keyFetchToken);
  }

  fetchAndUnwrapKeys(keyFetchToken) {
    return this._fxia.withCurrentAccountState(async currentState => {
      if (logPII) {
        log.debug("fetchAndUnwrapKeys: token: " + keyFetchToken);
      }
      // Sign out if we don't have a key fetch token.
      if (!keyFetchToken) {
        // this seems really bad and we should remove this - bug 1572313.
        log.warn("improper fetchAndUnwrapKeys() call: token missing");
        await this._fxia.signOut();
        return null;
      }

      let { wrapKB } = await this.fetchKeys(keyFetchToken);

      let data = await currentState.getUserAccountData();

      // Sanity check that the user hasn't changed out from under us (which
      // should be impossible given our _withCurrentAccountState, but...)
      if (data.keyFetchToken !== keyFetchToken) {
        throw new Error("Signed in user changed while fetching keys!");
      }

      let kBbytes = CryptoUtils.xor(
        CommonUtils.hexToBytes(data.unwrapBKey),
        wrapKB
      );

      if (logPII) {
        log.debug("kBbytes: " + kBbytes);
      }
      let updateData = {
        ...(await this._deriveKeys(data.uid, kBbytes)),
        keyFetchToken: null, // null values cause the item to be removed.
        unwrapBKey: null,
      };

      log.debug(
        "Keys Obtained:" +
          DERIVED_KEYS_NAMES.map(k => `${k}=${!!updateData[k]}`).join(", ")
      );
      if (logPII) {
        log.debug(
          "Keys Obtained:" +
            DERIVED_KEYS_NAMES.map(k => `${k}=${updateData[k]}`).join(", ")
        );
      }

      await currentState.updateUserAccountData(updateData);
      // Some parts of the device registration depend on the Sync keys being available,
      // so let's re-trigger it now that we have them.
      await this._fxia.updateDeviceRegistration();
      data = await currentState.getUserAccountData();
      return data;
    });
  }

  /**
   * @param {String} scope Single key bearing scope
   */
  async getKeyForScope(scope, { keyRotationTimestamp }) {
    if (scope !== SCOPE_OLD_SYNC) {
      throw new Error(`Unavailable key material for ${scope}`);
    }
    let { kSync, kXCS } = await this.getKeys();
    if (!kSync || !kXCS) {
      throw new Error("Could not find requested key.");
    }
    kXCS = ChromeUtils.base64URLEncode(CommonUtils.hexToArrayBuffer(kXCS), {
      pad: false,
    });
    kSync = ChromeUtils.base64URLEncode(CommonUtils.hexToArrayBuffer(kSync), {
      pad: false,
    });
    const kid = `${keyRotationTimestamp}-${kXCS}`;
    return {
      scope,
      kid,
      k: kSync,
      kty: "oct",
    };
  }

  /**
   * @param {String} scopes Space separated requested scopes
   */
  async getScopedKeys(scopes, clientId) {
    const { sessionToken } = await this._fxia._getVerifiedAccountOrReject();
    const keyData = await this._fxia.fxAccountsClient.getScopedKeyData(
      sessionToken,
      clientId,
      scopes
    );
    const scopedKeys = {};
    for (const [scope, data] of Object.entries(keyData)) {
      scopedKeys[scope] = await this.getKeyForScope(scope, data);
    }
    return scopedKeys;
  }

  async _deriveKeys(uid, kBbytes) {
    return {
      kSync: CommonUtils.bytesAsHex(await this._deriveSyncKey(kBbytes)),
      kXCS: CommonUtils.bytesAsHex(this._deriveXClientState(kBbytes)),
      kExtSync: CommonUtils.bytesAsHex(
        await this._deriveWebExtSyncStoreKey(kBbytes)
      ),
      kExtKbHash: CommonUtils.bytesAsHex(
        this._deriveWebExtKbHash(uid, kBbytes)
      ),
    };
  }

  /**
   * Derive the Sync Key given the byte string kB.
   *
   * @returns Promise<HKDF(kB, undefined, "identity.mozilla.com/picl/v1/oldsync", 64)>
   */
  _deriveSyncKey(kBbytes) {
    return CryptoUtils.hkdfLegacy(
      kBbytes,
      undefined,
      "identity.mozilla.com/picl/v1/oldsync",
      2 * 32
    );
  }

  /**
   * Invalidate the FxA certificate, so that it will be refreshed from the server
   * the next time it is needed.
   */
  invalidateCertificate() {
    return this._fxia.withCurrentAccountState(async state => {
      await state.updateUserAccountData({ cert: null });
    });
  }

  /**
   * Derive the WebExtensions Sync Storage Key given the byte string kB.
   *
   * @returns Promise<HKDF(kB, undefined, "identity.mozilla.com/picl/v1/chrome.storage.sync", 64)>
   */
  _deriveWebExtSyncStoreKey(kBbytes) {
    return CryptoUtils.hkdfLegacy(
      kBbytes,
      undefined,
      "identity.mozilla.com/picl/v1/chrome.storage.sync",
      2 * 32
    );
  }

  /**
   * Derive the WebExtensions kbHash given the byte string kB.
   *
   * @returns SHA256(uid + kB)
   */
  _deriveWebExtKbHash(uid, kBbytes) {
    return this._sha256(uid + kBbytes);
  }

  /**
   * Derive the X-Client-State header given the byte string kB.
   *
   * @returns SHA256(kB)[:16]
   */
  _deriveXClientState(kBbytes) {
    return this._sha256(kBbytes).slice(0, 16);
  }

  _sha256(bytes) {
    let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
    hasher.init(hasher.SHA256);
    return CryptoUtils.digestBytes(bytes, hasher);
  }
}

var EXPORTED_SYMBOLS = ["FxAccountsKeys"];
