/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.Logger;

import android.content.SharedPreferences;

public class PersistedCrypto5Keys {
  public static final String LOG_TAG = "PersistedC5Keys";

  public static final String CRYPTO5_KEYS_SERVER_RESPONSE_BODY = "crypto5KeysServerResponseBody";
  public static final String CRYPTO5_KEYS_LAST_MODIFIED        = "crypto5KeysLastModified";

  protected SharedPreferences prefs;
  protected KeyBundle syncKeyBundle;

  public PersistedCrypto5Keys(SharedPreferences prefs, KeyBundle syncKeyBundle) {
    if (syncKeyBundle == null) {
      throw new IllegalArgumentException("Null syncKeyBundle passed in to PersistedCrypto5Keys constructor.");
    }
    this.prefs = prefs;
    this.syncKeyBundle = syncKeyBundle;
  }

  /**
   * Get persisted crypto/keys.
   * <p>
   * crypto/keys is fetched from an encrypted JSON-encoded <code>CryptoRecord</code>.
   *
   * @return A <code>CollectionKeys</code> instance or <code>null</code> if none
   *         is currently persisted.
   */
  public CollectionKeys keys() {
    String keysJSON = prefs.getString(CRYPTO5_KEYS_SERVER_RESPONSE_BODY, null);
    if (keysJSON == null) {
      return null;
    }
    try {
      CryptoRecord cryptoRecord = CryptoRecord.fromJSONRecord(keysJSON);
      CollectionKeys keys = new CollectionKeys();
      keys.setKeyPairsFromWBO(cryptoRecord, syncKeyBundle);
      return keys;
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception decrypting persisted crypto/keys.", e);
      return null;
    }
  }

  /**
   * Persist crypto/keys.
   * <p>
   * crypto/keys is stored as an encrypted JSON-encoded <code>CryptoRecord</code>.
   *
   * @param keys
   *          The <code>CollectionKeys</code> object to persist, which should
   *          have the same default key bundle as the sync key bundle.
   */
  public void persistKeys(CollectionKeys keys) {
    if (keys == null) {
      Logger.debug(LOG_TAG, "Clearing persisted crypto/keys.");
      prefs.edit().remove(CRYPTO5_KEYS_SERVER_RESPONSE_BODY).commit();
      return;
    }
    try {
      CryptoRecord cryptoRecord = keys.asCryptoRecord();
      cryptoRecord.keyBundle = syncKeyBundle;
      cryptoRecord.encrypt();
      String keysJSON = cryptoRecord.toJSONString();
      Logger.debug(LOG_TAG, "Persisting crypto/keys.");
      prefs.edit().putString(CRYPTO5_KEYS_SERVER_RESPONSE_BODY, keysJSON).commit();
    } catch (Exception e) {
      Logger.warn(LOG_TAG, "Got exception encrypting while persisting crypto/keys.", e);
    }
  }

  public boolean persistedKeysExist() {
    return lastModified() > 0;
  }

  public long lastModified() {
    return prefs.getLong(CRYPTO5_KEYS_LAST_MODIFIED, -1);
  }

  public void persistLastModified(long lastModified) {
    if (lastModified <= 0) {
      Logger.debug(LOG_TAG, "Clearing persisted crypto/keys last modified timestamp.");
      prefs.edit().remove(CRYPTO5_KEYS_LAST_MODIFIED).commit();
      return;
    }
    Logger.debug(LOG_TAG, "Persisting crypto/keys last modified timestamp " + lastModified + ".");
    prefs.edit().putLong(CRYPTO5_KEYS_LAST_MODIFIED, lastModified).commit();
  }

  public void purge() {
    persistLastModified(-1);
    persistKeys(null);
  }
}
