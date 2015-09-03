/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.crypto.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.Before;
import org.junit.Test;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.CollectionKeys;
import org.mozilla.gecko.sync.NoCollectionKeysSetException;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.crypto.PersistedCrypto5Keys;

public class TestPersistedCrypto5Keys {
  MockSharedPreferences prefs = null;

  @Before
  public void setUp() {
    prefs = new MockSharedPreferences();
  }

  @Test
  public void testPersistLastModified() throws CryptoException, NoCollectionKeysSetException {
    long LAST_MODIFIED = System.currentTimeMillis();
    KeyBundle syncKeyBundle = KeyBundle.withRandomKeys();
    PersistedCrypto5Keys persisted = new PersistedCrypto5Keys(prefs, syncKeyBundle);

    // Test fresh start.
    assertEquals(-1, persisted.lastModified());

    // Test persisting.
    persisted.persistLastModified(LAST_MODIFIED);
    assertEquals(LAST_MODIFIED, persisted.lastModified());

    // Test clearing.
    persisted.persistLastModified(0);
    assertEquals(-1, persisted.lastModified());
  }

  @Test
  public void testPersistKeys() throws CryptoException, NoCollectionKeysSetException {
    KeyBundle syncKeyBundle = KeyBundle.withRandomKeys();
    KeyBundle testKeyBundle = KeyBundle.withRandomKeys();

    PersistedCrypto5Keys persisted = new PersistedCrypto5Keys(prefs, syncKeyBundle);

    // Test fresh start.
    assertNull(persisted.keys());

    // Test persisting.
    CollectionKeys keys = new CollectionKeys();
    keys.setDefaultKeyBundle(syncKeyBundle);
    keys.setKeyBundleForCollection("test", testKeyBundle);
    persisted.persistKeys(keys);

    CollectionKeys persistedKeys = persisted.keys();
    assertNotNull(persistedKeys);
    assertArrayEquals(syncKeyBundle.getEncryptionKey(), persistedKeys.defaultKeyBundle().getEncryptionKey());
    assertArrayEquals(syncKeyBundle.getHMACKey(), persistedKeys.defaultKeyBundle().getHMACKey());
    assertArrayEquals(testKeyBundle.getEncryptionKey(), persistedKeys.keyBundleForCollection("test").getEncryptionKey());
    assertArrayEquals(testKeyBundle.getHMACKey(), persistedKeys.keyBundleForCollection("test").getHMACKey());

    // Test clearing.
    persisted.persistKeys(null);
    assertNull(persisted.keys());

    // Test loading a persisted bundle with wrong syncKeyBundle.
    persisted.persistKeys(keys);
    assertNotNull(persisted.keys());

    persisted = new PersistedCrypto5Keys(prefs, testKeyBundle);
    assertNull(persisted.keys());
  }
}
