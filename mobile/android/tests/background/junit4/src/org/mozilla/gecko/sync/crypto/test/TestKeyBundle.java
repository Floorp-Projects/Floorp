/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.crypto.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.UnsupportedEncodingException;
import java.util.Arrays;

import org.junit.Test;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;

public class TestKeyBundle {
  @Test
  public void testCreateKeyBundle() throws UnsupportedEncodingException, CryptoException {
    String username              = "smqvooxj664hmrkrv6bw4r4vkegjhkns";
    String friendlyBase32SyncKey = "gbh7teqqcgyzd65svjgibd7tqy";
    String base64EncryptionKey   = "069EnS3EtDK4y1tZ1AyKX+U7WEsWRp9b" +
                                   "RIKLdW/7aoE=";
    String base64HmacKey         = "LF2YCS1QCgSNCf0BCQvQ06SGH8jqJDi9" +
                                   "dKj0O+b0fwI=";

    KeyBundle keys = new KeyBundle(username, friendlyBase32SyncKey);
    assertArrayEquals(keys.getEncryptionKey(), Base64.decodeBase64(base64EncryptionKey.getBytes("UTF-8")));
    assertArrayEquals(keys.getHMACKey(), Base64.decodeBase64(base64HmacKey.getBytes("UTF-8")));
  }

  /*
   * Basic sanity check to make sure length of keys is correct (32 bytes).
   * Also make sure that the two keys are different.
   */
  @Test
  public void testGenerateRandomKeys() throws CryptoException {
    KeyBundle keys = KeyBundle.withRandomKeys();

    assertEquals(32, keys.getEncryptionKey().length);
    assertEquals(32, keys.getHMACKey().length);

    boolean equal = Arrays.equals(keys.getEncryptionKey(), keys.getHMACKey());
    assertEquals(false, equal);
  }

  @Test
  public void testEquals() throws CryptoException {
    KeyBundle k = KeyBundle.withRandomKeys();
    KeyBundle o = KeyBundle.withRandomKeys();
    assertFalse(k.equals("test"));
    assertFalse(k.equals(o));
    assertTrue(k.equals(k));
    assertTrue(o.equals(o));
    o.setHMACKey(k.getHMACKey());
    assertFalse(o.equals(k));
    o.setEncryptionKey(k.getEncryptionKey());
    assertTrue(o.equals(k));
  }
}
