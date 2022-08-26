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

const {
  LEGACY_DERIVED_KEYS_NAMES,
  SCOPE_OLD_SYNC,
  LEGACY_SCOPE_WEBEXT_SYNC,
  DEPRECATED_SCOPE_ECOSYSTEM_TELEMETRY,
  FX_OAUTH_CLIENT_ID,
  log,
  logPII,
} = ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js");

// These are the scopes that correspond to new storage for the `LEGACY_DERIVED_KEYS_NAMES`.
// We will, if necessary, migrate storage for those keys so that it's associated with
// these scopes.
const LEGACY_DERIVED_KEY_SCOPES = [SCOPE_OLD_SYNC, LEGACY_SCOPE_WEBEXT_SYNC];

// These are scopes that we used to store, but are no longer using,
// and hence should be deleted from storage if present.
const DEPRECATED_KEY_SCOPES = [DEPRECATED_SCOPE_ECOSYSTEM_TELEMETRY];

/**
 * Utilities for working with key material linked to the user's account.
 *
 * Each Firefox Account has 32 bytes of root key material called `kB` which is
 * linked to the user's password, and which is used to derive purpose-specific
 * subkeys for things like encrypting the user's sync data. This class provides
 * the interface for working with such key material.
 *
 * Most recent FxA clients obtain appropriate key material directly as part of
 * their sign-in flow, using a special extension of the OAuth2.0 protocol to
 * securely deliver the derived keys without revealing `kB`. Keys obtained in
 * in this way are called "scoped keys" since each corresponds to a particular
 * OAuth scope, and this class provides a `getKeyForScope` method that is the
 * preferred method for consumers to work with such keys.
 *
 * However, since the FxA integration in Firefox Desktop pre-dates the use of
 * OAuth2.0, we also have a lot of code for fetching keys via an older flow.
 * This flow uses a special `keyFetchToken` to obtain `kB` and then derive various
 * sub-keys from it. Consumers should consider this an internal implementation
 * detail of the `FxAccountsKeys` class and should prefer `getKeyForScope` where
 * possible.  We intend to remove support for Firefox ever directly handling `kB`
 * at some point in the future.
 */
class FxAccountsKeys {
  constructor(fxAccountsInternal) {
    this._fxai = fxAccountsInternal;
  }

  /**
   * Checks if we currently have the key for a given scope, or if we have enough to
   * be able to successfully fetch and unwrap it for the signed-in-user.
   *
   * Unlike `getKeyForScope`, this will not hit the network to fetch wrapped keys if
   * they aren't available locally.
   */
  canGetKeyForScope(scope) {
    return this._fxai.withCurrentAccountState(async currentState => {
      let userData = await currentState.getUserAccountData();
      if (!userData) {
        throw new Error("Can't possibly get keys; User is not signed in");
      }
      if (!userData.verified) {
        log.info("Can't get keys; user is not verified");
        return false;
      }

      if (userData.scopedKeys && userData.scopedKeys.hasOwnProperty(scope)) {
        return true;
      }

      // For sync-related scopes, we might have stored the keys in a legacy format.
      if (scope == SCOPE_OLD_SYNC) {
        if (userData.kSync && userData.kXCS) {
          return true;
        }
      }
      if (scope == LEGACY_SCOPE_WEBEXT_SYNC) {
        if (userData.kExtSync && userData.kExtKbHash) {
          return true;
        }
      }

      // `kB` is deprecated, but if we have it, we can use it to derive any scoped key.
      if (userData.kB) {
        return true;
      }

      // If we have a `keyFetchToken` we can fetch `kB`.
      if (userData.keyFetchToken) {
        return true;
      }

      log.info("Can't get keys; no key material or tokens available");
      return false;
    });
  }

  /**
   * Get the key for a specified OAuth scope.
   *
   * @param {String} scope The OAuth scope whose key should be returned
   *
   * @return Promise<JWK>
   *        If no key is available the promise resolves to `null`.
   *        If a key is available for the given scope, th promise resolves to a JWK with fields:
   *        {
   *          scope: The requested scope
   *          kid: Key identifier
   *          k: Derived key material
   *          kty: Always "oct" for scoped keys
   *        }
   *
   */
  async getKeyForScope(scope) {
    const { scopedKeys } = await this._loadOrFetchKeys();
    if (!scopedKeys.hasOwnProperty(scope)) {
      throw new Error(`Key not available for scope "${scope}"`);
    }
    return {
      scope,
      ...scopedKeys[scope],
    };
  }

  /**
   * Format a JWK key material as hex rather than base64.
   *
   * This is a backwards-compatibility helper for code that needs raw key bytes rather
   * than the JWK format offered by FxA scopes keys.
   *
   * @param {Object} jwk The JWK from which to extract the `k` field as hex.
   *
   */
  keyAsHex(jwk) {
    return CommonUtils.base64urlToHex(jwk.k);
  }

  /**
   * Format a JWK kid as hex rather than base64.
   *
   * This is a backwards-compatibility helper for code that needs a raw key fingerprint
   * for use as a key identifier, rather than the timestamp+fingerprint format used by
   * FxA scoped keys.
   *
   * @param {Object} jwk The JWK from which to extract the `kid` field as hex.
   */
  kidAsHex(jwk) {
    // The kid format is "{timestamp}-{b64url(fingerprint)}", but we have to be careful
    // because the fingerprint component may contain "-" as well, and we want to ensure
    // the timestamp component was non-empty.
    const idx = jwk.kid.indexOf("-") + 1;
    if (idx <= 1) {
      throw new Error(`Invalid kid: ${jwk.kid}`);
    }
    return CommonUtils.base64urlToHex(jwk.kid.slice(idx));
  }

  /**
   * Fetch encryption keys for the signed-in-user from the FxA API server.
   *
   * Not for user consumption.  Exists to cause the keys to be fetched.
   *
   * Returns user data so that it can be chained with other methods.
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          scopedKeys: Object mapping OAuth scopes to corresponding derived keys
   *          kSync: An encryption key for Sync
   *          kXCS: A key hash of kB for the X-Client-State header
   *          kExtSync: An encryption key for WebExtensions syncing
   *          kExtKbHash: A key hash of kB for WebExtensions syncing
   *          verified: email verification status
   *        }
   * @throws If there is no user signed in.
   */
  async _loadOrFetchKeys() {
    return this._fxai.withCurrentAccountState(async currentState => {
      try {
        let userData = await currentState.getUserAccountData();
        if (!userData) {
          throw new Error("Can't get keys; User is not signed in");
        }
        // If we have all the keys in latest storage location, we're good.
        if (userData.scopedKeys) {
          if (
            LEGACY_DERIVED_KEY_SCOPES.every(scope =>
              userData.scopedKeys.hasOwnProperty(scope)
            ) &&
            !DEPRECATED_KEY_SCOPES.some(scope =>
              userData.scopedKeys.hasOwnProperty(scope)
            )
          ) {
            return userData;
          }
        }
        // If not, we've got work to do, and we debounce to avoid duplicating it.
        if (!currentState.whenKeysReadyDeferred) {
          currentState.whenKeysReadyDeferred = PromiseUtils.defer();
          // N.B. we deliberately don't `await` here, and instead use the promise
          // to resolve `whenKeysReadyDeferred` (which we then `await` below).
          this._migrateOrFetchKeys(currentState, userData).then(
            dataWithKeys => {
              currentState.whenKeysReadyDeferred.resolve(dataWithKeys);
              currentState.whenKeysReadyDeferred = null;
            },
            err => {
              currentState.whenKeysReadyDeferred.reject(err);
              currentState.whenKeysReadyDeferred = null;
            }
          );
        }
        return await currentState.whenKeysReadyDeferred.promise;
      } catch (err) {
        return this._fxai._handleTokenError(err);
      }
    });
  }

  /**
   * Key storage migration or fetching logic.
   *
   * This method contains the doing-expensive-operations part of the logic of
   * _loadOrFetchKeys(), factored out into a separate method so we can debounce it.
   *
   */
  async _migrateOrFetchKeys(currentState, userData) {
    // Bug 1697596 - delete any deprecated scoped keys from storage.
    // If any of the deprecated keys are present, then we know that we've
    // previously applied all the other migrations below, otherwise there
    // would not be any `scopedKeys` field.
    if (userData.scopedKeys) {
      const toRemove = DEPRECATED_KEY_SCOPES.filter(scope =>
        userData.scopedKeys.hasOwnProperty(scope)
      );
      if (toRemove.length) {
        for (const scope of toRemove) {
          delete userData.scopedKeys[scope];
        }
        await currentState.updateUserAccountData({
          scopedKeys: userData.scopedKeys,
          // Prior to deprecating SCOPE_ECOSYSTEM_TELEMETRY, this file had some
          // special code to store it as a top-level user data field. So, this
          // file also gets to delete it as part of the deprecation.
          ecosystemUserId: null,
          ecosystemAnonId: null,
        });
        userData = await currentState.getUserAccountData();
        return userData;
      }
    }
    // Bug 1661407 - migrate from legacy storage of keys as top-level account
    // data fields, to storing them as scoped keys in the `scopedKeys` object.
    if (
      LEGACY_DERIVED_KEYS_NAMES.every(name => userData.hasOwnProperty(name))
    ) {
      log.info("Migrating from legacy key fields to scopedKeys.");
      const scopedKeys = userData.scopedKeys || {};
      await currentState.updateUserAccountData({
        scopedKeys: {
          ...scopedKeys,
          ...(await this._deriveScopedKeysFromAccountData(userData)),
        },
      });
      userData = await currentState.getUserAccountData();
      return userData;
    }
    // Bug 1426306 - Migrate from kB to derived keys.
    if (userData.kB) {
      log.info("Migrating kB to derived keys.");
      const { uid, kB, sessionToken } = userData;
      const scopedKeysMetadata = await this._fetchScopedKeysMetadata(
        sessionToken
      );
      await currentState.updateUserAccountData({
        uid,
        ...(await this._deriveKeys(
          uid,
          CommonUtils.hexToBytes(kB),
          scopedKeysMetadata
        )),
        kA: null, // Remove kA and kB from storage.
        kB: null,
      });
      userData = await currentState.getUserAccountData();
      return userData;
    }
    // Otherwise, we need to fetch from the network and unwrap.
    if (!userData.sessionToken) {
      throw new Error("No sessionToken");
    }
    if (!userData.keyFetchToken) {
      throw new Error("No keyFetchToken");
    }
    return this._fetchAndUnwrapAndDeriveKeys(
      currentState,
      userData.sessionToken,
      userData.keyFetchToken
    );
  }

  /**
   * Fetch keys from the server, unwrap them, and derive required sub-keys.
   *
   * Once the user's email is verified, we can resquest the root key `kB` from the
   * FxA server, unwrap it using the client-side secret `unwrapBKey`, and then
   * derive all the sub-keys required for operation of the browser.
   */
  async _fetchAndUnwrapAndDeriveKeys(
    currentState,
    sessionToken,
    keyFetchToken
  ) {
    if (logPII) {
      log.debug(
        `fetchAndUnwrapKeys: sessionToken: ${sessionToken}, keyFetchToken: ${keyFetchToken}`
      );
    }

    // Sign out if we don't have the necessary tokens.
    if (!sessionToken || !keyFetchToken) {
      // this seems really bad and we should remove this - bug 1572313.
      log.warn("improper _fetchAndUnwrapKeys() call: token missing");
      await this._fxai.signOut();
      return null;
    }

    // Deriving OAuth scoped keys requires additional metadata from the server.
    // We fetch this first, before fetching the actual key material, because the
    // keyFetchToken is single-use and we don't want to do a potentially-fallible
    // operation after consuming it.
    const scopedKeysMetadata = await this._fetchScopedKeysMetadata(
      sessionToken
    );

    // Fetch the wrapped keys.
    // It would be nice to be able to fetch this in a single operation with fetching
    // the metadata above, but that requires server-side changes in FxA.
    let { wrapKB } = await this._fetchKeys(keyFetchToken);

    let data = await currentState.getUserAccountData();

    // Sanity check that the user hasn't changed out from under us (which should
    // be impossible given this is called within _withCurrentAccountState, but...)
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
      ...(await this._deriveKeys(data.uid, kBbytes, scopedKeysMetadata)),
      keyFetchToken: null, // null values cause the item to be removed.
      unwrapBKey: null,
    };

    if (logPII) {
      log.debug(`Keys Obtained: ${updateData.scopedKeys}`);
    } else {
      log.debug(
        "Keys Obtained: " + Object.keys(updateData.scopedKeys).join(", ")
      );
    }

    // Just double-check that we derived all the right stuff.
    const EXPECTED_FIELDS = LEGACY_DERIVED_KEYS_NAMES.concat(["scopedKeys"]);
    if (EXPECTED_FIELDS.some(k => !updateData[k])) {
      const missing = EXPECTED_FIELDS.filter(k => !updateData[k]);
      throw new Error(`user data missing: ${missing.join(", ")}`);
    }

    await currentState.updateUserAccountData(updateData);
    return currentState.getUserAccountData();
  }

  /**
   * Fetch the wrapped root key `wrapKB` from the FxA server.
   *
   * This consumes the single-use `keyFetchToken`.
   */
  _fetchKeys(keyFetchToken) {
    let client = this._fxai.fxAccountsClient;
    log.debug(
      `Fetching keys with token ${!!keyFetchToken} from ${client.host}`
    );
    if (logPII) {
      log.debug("fetchKeys - the token is " + keyFetchToken);
    }
    return client.accountKeys(keyFetchToken);
  }

  /**
   * Fetch additional metadata required for deriving scoped keys.
   *
   * This includes timestamps and a server-provided secret to mix in to
   * the derived value in order to support key rotation.
   */
  async _fetchScopedKeysMetadata(sessionToken) {
    // Hard-coded list of scopes that we know about.
    // This list will probably grow in future.
    // Note that LEGACY_SCOPE_WEBEXT_SYNC is not in this list, it gets special-case handling below.
    const scopes = [SCOPE_OLD_SYNC].join(" ");
    const scopedKeysMetadata = await this._fxai.fxAccountsClient.getScopedKeyData(
      sessionToken,
      FX_OAUTH_CLIENT_ID,
      scopes
    );
    // The server may decline us permission for some of those scopes, although it really shouldn't.
    // We can live without them...except for the OLDSYNC scope, whose absence would be catastrophic.
    if (!scopedKeysMetadata.hasOwnProperty(SCOPE_OLD_SYNC)) {
      log.warn(
        "The FxA server did not grant Firefox the `oldsync` scope; this is most unexpected!" +
          ` scopes were: ${Object.keys(scopedKeysMetadata)}`
      );
      throw new Error(
        "The FxA server did not grant Firefox the `oldsync` scope"
      );
    }
    // Firefox Desktop invented its own special scope for legacy webextension syncing,
    // with its own special key. Rather than teach the rest of FxA about this scope
    // that will never be used anywhere else, just give it the same metadata as
    // the main sync scope. This can go away once legacy webext sync is removed.
    // (ref Bug 1637465 for tracking that removal)
    scopedKeysMetadata[LEGACY_SCOPE_WEBEXT_SYNC] = {
      ...scopedKeysMetadata[SCOPE_OLD_SYNC],
      identifier: LEGACY_SCOPE_WEBEXT_SYNC,
    };
    return scopedKeysMetadata;
  }

  /**
   * Derive purpose-specific keys from the root FxA key `kB`.
   *
   * Everything that uses an encryption key from FxA uses a purpose-specific derived
   * key. For new uses this is derived in a structured way based on OAuth scopes,
   * while for legacy uses (mainly Firefox Sync) it is derived in a more ad-hoc fashion.
   * This method does all the derivations for the uses that we know about.
   *
   */
  async _deriveKeys(uid, kBbytes, scopedKeysMetadata) {
    const scopedKeys = await this._deriveScopedKeys(
      uid,
      kBbytes,
      scopedKeysMetadata
    );
    return {
      scopedKeys,
      // Existing browser code might expect sync keys to be available as top-level account data.
      // For b/w compat we can derive these even if they're not in our list of scoped keys for
      // some reason (since the derivation doesn't depend on server-provided data).
      kSync: scopedKeys[SCOPE_OLD_SYNC]
        ? this.keyAsHex(scopedKeys[SCOPE_OLD_SYNC])
        : CommonUtils.bytesAsHex(await this._deriveSyncKey(kBbytes)),
      kXCS: scopedKeys[SCOPE_OLD_SYNC]
        ? this.kidAsHex(scopedKeys[SCOPE_OLD_SYNC])
        : CommonUtils.bytesAsHex(await this._deriveXClientState(kBbytes)),
      kExtSync: scopedKeys[LEGACY_SCOPE_WEBEXT_SYNC]
        ? this.keyAsHex(scopedKeys[LEGACY_SCOPE_WEBEXT_SYNC])
        : CommonUtils.bytesAsHex(await this._deriveWebExtSyncStoreKey(kBbytes)),
      kExtKbHash: scopedKeys[LEGACY_SCOPE_WEBEXT_SYNC]
        ? this.kidAsHex(scopedKeys[LEGACY_SCOPE_WEBEXT_SYNC])
        : CommonUtils.bytesAsHex(await this._deriveWebExtKbHash(uid, kBbytes)),
    };
  }

  /**
   * Derive various scoped keys from the root FxA key `kB`.
   *
   * The `scopedKeysMetadata` object is additional information fetched from the server that
   * that gets mixed in to the key derivation, with each member of the object corresponding
   * to an OAuth scope that keys its own scoped key.
   *
   * As a special case for backwards-compatibility, sync-related scopes get special
   * treatment to use a legacy derivation algorithm.
   *
   */
  async _deriveScopedKeys(uid, kBbytes, scopedKeysMetadata) {
    const scopedKeys = {};
    for (const scope in scopedKeysMetadata) {
      if (LEGACY_DERIVED_KEY_SCOPES.includes(scope)) {
        scopedKeys[scope] = await this._deriveLegacyScopedKey(
          uid,
          kBbytes,
          scope,
          scopedKeysMetadata[scope]
        );
      } else {
        scopedKeys[scope] = await this._deriveScopedKey(
          uid,
          kBbytes,
          scope,
          scopedKeysMetadata[scope]
        );
      }
    }
    return scopedKeys;
  }

  /**
   * Derive the `scopedKeys` data field based on current account data.
   *
   * This is a backwards-compatibility convenience for users who are already signed in to Firefox
   * and have legacy fields like `kSync` and `kXCS` in their top-level account data, but do not have
   * the newer `scopedKeys` field. We populate it with the scoped keys for sync and webext-sync.
   *
   */
  async _deriveScopedKeysFromAccountData(userData) {
    const scopedKeysMetadata = await this._fetchScopedKeysMetadata(
      userData.sessionToken
    );
    const scopedKeys = userData.scopedKeys || {};
    for (const scope of LEGACY_DERIVED_KEY_SCOPES) {
      if (scopedKeysMetadata.hasOwnProperty(scope)) {
        let kid, key;
        if (scope == SCOPE_OLD_SYNC) {
          ({ kXCS: kid, kSync: key } = userData);
        } else if (scope == LEGACY_SCOPE_WEBEXT_SYNC) {
          ({ kExtKbHash: kid, kExtSync: key } = userData);
        } else {
          // Should never happen, but a nice internal consistency check.
          throw new Error(`Unexpected legacy key-bearing scope: ${scope}`);
        }
        if (!kid || !key) {
          throw new Error(
            `Account is missing legacy key fields for scope: ${scope}`
          );
        }
        scopedKeys[scope] = await this._formatLegacyScopedKey(
          CommonUtils.hexToArrayBuffer(kid),
          CommonUtils.hexToArrayBuffer(key),
          scope,
          scopedKeysMetadata[scope]
        );
      }
    }
    return scopedKeys;
  }

  /**
   * Derive a scoped key for an individual OAuth scope.
   *
   * The derivation here uses HKDF to combine:
   *   - the root key material kB
   *   - a unique identifier for this scoped key
   *   - a server-provided secret that allows for key rotation
   *   - the account uid as an additional salt
   *
   * It produces 32 bytes of (secret) key material along with a (potentially public)
   * key identifier, formatted as a JWK.
   *
   * The full details are in the technical docs at
   * https://docs.google.com/document/d/1IvQJFEBFz0PnL4uVlIvt8fBS_IPwSK-avK0BRIHucxQ/
   */
  async _deriveScopedKey(uid, kBbytes, scope, scopedKeyMetadata) {
    kBbytes = CommonUtils.byteStringToArrayBuffer(kBbytes);

    const FINGERPRINT_LENGTH = 16;
    const KEY_LENGTH = 32;
    const VALID_UID = /^[0-9a-f]{32}$/i;
    const VALID_ROTATION_SECRET = /^[0-9a-f]{64}$/i;

    // Engage paranoia mode for input data.
    if (!VALID_UID.test(uid)) {
      throw new Error("uid must be a 32-character hex string");
    }
    if (kBbytes.length != 32) {
      throw new Error("kBbytes must be exactly 32 bytes");
    }
    if (
      typeof scopedKeyMetadata.identifier !== "string" ||
      scopedKeyMetadata.identifier.length < 10
    ) {
      throw new Error("identifier must be a string of length >= 10");
    }
    if (typeof scopedKeyMetadata.keyRotationTimestamp !== "number") {
      throw new Error("keyRotationTimestamp must be a number");
    }
    if (!VALID_ROTATION_SECRET.test(scopedKeyMetadata.keyRotationSecret)) {
      throw new Error("keyRotationSecret must be a 64-character hex string");
    }

    // The server returns milliseconds, we want seconds as a string.
    const keyRotationTimestamp =
      "" + Math.round(scopedKeyMetadata.keyRotationTimestamp / 1000);
    if (keyRotationTimestamp.length < 10) {
      throw new Error("keyRotationTimestamp must round to a 10-digit number");
    }

    const keyRotationSecret = CommonUtils.hexToArrayBuffer(
      scopedKeyMetadata.keyRotationSecret
    );
    const salt = CommonUtils.hexToArrayBuffer(uid);
    const context = new TextEncoder("utf8").encode(
      "identity.mozilla.com/picl/v1/scoped_key\n" + scopedKeyMetadata.identifier
    );

    const inputKey = new Uint8Array(64);
    inputKey.set(kBbytes, 0);
    inputKey.set(keyRotationSecret, 32);

    const derivedKeyMaterial = await CryptoUtils.hkdf(
      inputKey,
      salt,
      context,
      FINGERPRINT_LENGTH + KEY_LENGTH
    );
    const fingerprint = derivedKeyMaterial.slice(0, FINGERPRINT_LENGTH);
    const key = derivedKeyMaterial.slice(
      FINGERPRINT_LENGTH,
      FINGERPRINT_LENGTH + KEY_LENGTH
    );

    return {
      kid:
        keyRotationTimestamp +
        "-" +
        ChromeUtils.base64URLEncode(fingerprint, {
          pad: false,
        }),
      k: ChromeUtils.base64URLEncode(key, {
        pad: false,
      }),
      kty: "oct",
    };
  }

  /**
   * Derive the scoped key for the one of our legacy sync-related scopes.
   *
   * These uses a different key-derivation algoritm that incorporates less server-provided
   * data, for backwards-compatibility reasons.
   *
   */
  async _deriveLegacyScopedKey(uid, kBbytes, scope, scopedKeyMetadata) {
    let kid, key;
    if (scope == SCOPE_OLD_SYNC) {
      kid = await this._deriveXClientState(kBbytes);
      key = await this._deriveSyncKey(kBbytes);
    } else if (scope == LEGACY_SCOPE_WEBEXT_SYNC) {
      kid = await this._deriveWebExtKbHash(uid, kBbytes);
      key = await this._deriveWebExtSyncStoreKey(kBbytes);
    } else {
      throw new Error(`Unexpected legacy key-bearing scope: ${scope}`);
    }
    kid = CommonUtils.byteStringToArrayBuffer(kid);
    key = CommonUtils.byteStringToArrayBuffer(key);
    return this._formatLegacyScopedKey(kid, key, scope, scopedKeyMetadata);
  }

  /**
   * Format key material for a legacy scyne-related scope as a JWK.
   *
   * @param {ArrayBuffer} kid bytes of the key hash to use in the key identifier
   * @param {ArrayBuffer} key bytes of the derived sync key
   * @param {String} scope the scope with which this key is associated
   * @param {Number} keyRotationTimestamp server-provided timestamp of last key rotation
   * @returns {Object} key material formatted as a JWK object
   */
  _formatLegacyScopedKey(kid, key, scope, { keyRotationTimestamp }) {
    kid = ChromeUtils.base64URLEncode(kid, {
      pad: false,
    });
    key = ChromeUtils.base64URLEncode(key, {
      pad: false,
    });
    return {
      kid: `${keyRotationTimestamp}-${kid}`,
      k: key,
      kty: "oct",
    };
  }

  /**
   * Derive the Sync Key given the byte string kB.
   *
   * @returns Promise<HKDF(kB, undefined, "identity.mozilla.com/picl/v1/oldsync", 64)>
   */
  async _deriveSyncKey(kBbytes) {
    return CryptoUtils.hkdfLegacy(
      kBbytes,
      undefined,
      "identity.mozilla.com/picl/v1/oldsync",
      2 * 32
    );
  }

  /**
   * Derive the X-Client-State header given the byte string kB.
   *
   * @returns Promise<SHA256(kB)[:16]>
   */
  async _deriveXClientState(kBbytes) {
    return this._sha256(kBbytes).slice(0, 16);
  }

  /**
   * Derive the WebExtensions Sync Storage Key given the byte string kB.
   *
   * @returns Promise<HKDF(kB, undefined, "identity.mozilla.com/picl/v1/chrome.storage.sync", 64)>
   */
  async _deriveWebExtSyncStoreKey(kBbytes) {
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
   * @returns Promise<SHA256(uid + kB)>
   */
  async _deriveWebExtKbHash(uid, kBbytes) {
    return this._sha256(uid + kBbytes);
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
