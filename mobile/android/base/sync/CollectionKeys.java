/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.HashMap;
import java.util.Map.Entry;

import org.json.simple.JSONArray;
import org.json.simple.parser.ParseException;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;

import android.util.Log;

public class CollectionKeys {
  private static final String LOG_TAG = "CollectionKeys";
  private KeyBundle                  defaultKeyBundle     = null;
  private HashMap<String, KeyBundle> collectionKeyBundles = new HashMap<String, KeyBundle>();

  public static CryptoRecord generateCollectionKeysRecord() throws CryptoException {
    CollectionKeys ck = generateCollectionKeys();
    try {
      return ck.asCryptoRecord();
    } catch (NoCollectionKeysSetException e) {
      // Cannot occur.
      Log.e(LOG_TAG, "generateCollectionKeys returned a value with no default key. Unpossible.", e);
      throw new IllegalStateException("CollectionKeys should not have null default key.");
    }
  }

  /**
   * Randomly generate a basic CollectionKeys object.
   * @throws CryptoException
   */
  public static CollectionKeys generateCollectionKeys() throws CryptoException {
    CollectionKeys ck = new CollectionKeys();
    ck.clear();
    ck.defaultKeyBundle = KeyBundle.withRandomKeys();
    // TODO: eventually we would like to keep per-collection keys, just generate
    // new ones as appropriate.
    return ck;
  }

  public KeyBundle defaultKeyBundle() throws NoCollectionKeysSetException {
    if (this.defaultKeyBundle == null) {
      throw new NoCollectionKeysSetException();
    }
    return this.defaultKeyBundle;
  }

  public KeyBundle keyBundleForCollection(String collection)
                                                      throws NoCollectionKeysSetException {
    if (this.defaultKeyBundle == null) {
      throw new NoCollectionKeysSetException();
    }
    if (collectionKeyBundles.containsKey(collection)) {
      return collectionKeyBundles.get(collection);
    }
    return this.defaultKeyBundle;
  }

  /**
   * Take a pair of values in a JSON array, handing them off to KeyBundle to
   * produce a usable keypair.
   */
  private static KeyBundle arrayToKeyBundle(JSONArray array) throws UnsupportedEncodingException {
    String encKeyStr  = (String) array.get(0);
    String hmacKeyStr = (String) array.get(1);
    return KeyBundle.fromBase64EncodedKeys(encKeyStr, hmacKeyStr);
  }

  @SuppressWarnings("unchecked")
  private static JSONArray keyBundleToArray(KeyBundle bundle) {
    // Generate JSON.
    JSONArray keysArray = new JSONArray();
    keysArray.add(new String(Base64.encodeBase64(bundle.getEncryptionKey())));
    keysArray.add(new String(Base64.encodeBase64(bundle.getHMACKey())));
    return keysArray;
  }

  private ExtendedJSONObject asRecordContents() throws NoCollectionKeysSetException {
    ExtendedJSONObject json = new ExtendedJSONObject();
    json.put("id", "keys");
    json.put("collection", "crypto");
    json.put("default", keyBundleToArray(this.defaultKeyBundle()));
    ExtendedJSONObject colls = new ExtendedJSONObject();
    for (Entry<String, KeyBundle> collKey : collectionKeyBundles.entrySet()) {
      colls.put(collKey.getKey(), keyBundleToArray(collKey.getValue()));
    }
    json.put("collections", colls);
    return json;
  }

  public CryptoRecord asCryptoRecord() throws NoCollectionKeysSetException {
    ExtendedJSONObject payload = this.asRecordContents();
    CryptoRecord record = new CryptoRecord(payload);
    record.collection = "crypto";
    record.guid       = "keys";
    record.deleted    = false;
    return record;
  }

  public static CollectionKeys fromCryptoRecord(CryptoRecord keys, KeyBundle syncKeyBundle) throws CryptoException, IOException, ParseException, NonObjectJSONException {
    if (syncKeyBundle != null) {
      keys.keyBundle = syncKeyBundle;
      keys.decrypt();
    }
    ExtendedJSONObject cleartext = keys.payload;
    KeyBundle defaultKey = arrayToKeyBundle((JSONArray) cleartext.get("default"));

    ExtendedJSONObject collections = cleartext.getObject("collections");
    HashMap<String, KeyBundle> collectionKeys = new HashMap<String, KeyBundle>();
    for (Entry<String, Object> pair : collections.entryIterable()) {
      KeyBundle bundle = arrayToKeyBundle((JSONArray) pair.getValue());
      collectionKeys.put(pair.getKey(), bundle);
    }

    CollectionKeys ck = new CollectionKeys();
    ck.collectionKeyBundles = collectionKeys;
    ck.defaultKeyBundle     = defaultKey;
    return ck;
  }

  /**
   * Take a downloaded record, and the Sync Key, decrypting the record and
   * setting our own keys accordingly.
   */
  public void setKeyPairsFromWBO(CryptoRecord keys, KeyBundle syncKeyBundle)
                                                                            throws CryptoException,
                                                                            IOException,
                                                                            ParseException,
                                                                            NonObjectJSONException {
    keys.keyBundle = syncKeyBundle;
    keys.decrypt();
    ExtendedJSONObject cleartext = keys.payload;
    KeyBundle defaultKey = arrayToKeyBundle((JSONArray) cleartext.get("default"));

    ExtendedJSONObject collections = cleartext.getObject("collections");
    HashMap<String, KeyBundle> collectionKeys = new HashMap<String, KeyBundle>();
    for (Entry<String, Object> pair : collections.entryIterable()) {
      KeyBundle bundle = arrayToKeyBundle((JSONArray) pair.getValue());
      collectionKeys.put(pair.getKey(), bundle);
    }

    this.collectionKeyBundles = collectionKeys;
    this.defaultKeyBundle     = defaultKey;
  }

  public void setKeyBundleForCollection(String collection, KeyBundle keys) {
    this.collectionKeyBundles.put(collection, keys);
  }

  public void setDefaultKeyBundle(KeyBundle keys) {
    this.defaultKeyBundle = keys;
  }

  public void clear() {
    this.defaultKeyBundle = null;
    this.collectionKeyBundles = new HashMap<String, KeyBundle>();
  }
}
