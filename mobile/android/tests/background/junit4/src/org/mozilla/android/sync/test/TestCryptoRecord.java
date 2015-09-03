/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.Arrays;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.ParseException;
import org.junit.Test;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

public class TestCryptoRecord {
  String base64EncryptionKey = "9K/wLdXdw+nrTtXo4ZpECyHFNr4d7aYHqeg3KW9+m6Q=";
  String base64HmacKey = "MMntEfutgLTc8FlTLQFms8/xMPmCldqPlq/QQXEjx70=";

  @Test
  public void testBaseCryptoRecordEncrypt() throws IOException, ParseException, NonObjectJSONException, CryptoException {
    ExtendedJSONObject clearPayload = ExtendedJSONObject.parseJSONObject("{\"id\":\"5qRsgXWRJZXr\",\"title\":\"Index of file:///Users/jason/Library/Application Support/Firefox/Profiles/ksgd7wpk.LocalSyncServer/weave/logs/\",\"histUri\":\"file:///Users/jason/Library/Application%20Support/Firefox/Profiles/ksgd7wpk.LocalSyncServer/weave/logs/\",\"visits\":[{\"type\":1,\"date\":1319149012372425}]}");

    CryptoRecord record = new CryptoRecord();
    record.payload = clearPayload;
    String expectedGUID = "5qRsgXWRJZXr";
    record.guid = expectedGUID;
    record.keyBundle = KeyBundle.fromBase64EncodedKeys(base64EncryptionKey, base64HmacKey);
    record.encrypt();
    assertTrue(record.payload.get("title") == null);
    assertTrue(record.payload.get("ciphertext") != null);
    assertEquals(expectedGUID, record.guid);
    assertEquals(expectedGUID, record.toJSONObject().get("id"));
    record.decrypt();
    assertEquals(expectedGUID, record.toJSONObject().get("id"));
  }

  @Test
  public void testEntireRecord() throws Exception {
    // Check a raw JSON blob from a real Sync account.
    String inputString = "{\"sortindex\": 131, \"payload\": \"{\\\"ciphertext\\\":\\\"YJB4dr0vZEIWPirfU2FCJvfzeSLiOP5QWasol2R6ILUxdHsJWuUuvTZVhxYQfTVNou6hVV67jfAvi5Cs+bqhhQsv7icZTiZhPTiTdVGt+uuMotxauVA5OryNGVEZgCCTvT3upzhDFdDbJzVd9O3/gU/b7r/CmAHykX8bTlthlbWeZ8oz6gwHJB5tPRU15nM/m/qW1vyKIw5pw/ZwtAy630AieRehGIGDk+33PWqsfyuT4EUFY9/Ly+8JlnqzxfiBCunIfuXGdLuqTjJOxgrK8mI4wccRFEdFEnmHvh5x7fjl1ID52qumFNQl8zkB75C8XK25alXqwvRR6/AQSP+BgQ==\\\",\\\"IV\\\":\\\"v/0BFgicqYQsd70T39rraA==\\\",\\\"hmac\\\":\\\"59605ed696f6e0e6e062a03510cff742bf6b50d695c042e8372a93f4c2d37dac\\\"}\", \"id\": \"0-P9fabp9vJD\", \"modified\": 1326254123.65}";
    CryptoRecord record = CryptoRecord.fromJSONRecord(inputString);
    assertEquals("0-P9fabp9vJD", record.guid);
    assertEquals(1326254123650L, record.lastModified);
    assertEquals(131,            record.sortIndex);

    String b64E = "0A7mU5SZ/tu7ZqwXW1og4qHVHN+zgEi4Xwfwjw+vEJw=";
    String b64H = "11GN34O9QWXkjR06g8t0gWE1sGgQeWL0qxxWwl8Dmxs=";
    record.keyBundle = KeyBundle.fromBase64EncodedKeys(b64E, b64H);
    record.decrypt();

    assertEquals("0-P9fabp9vJD", record.guid);
    assertEquals(1326254123650L, record.lastModified);
    assertEquals(131,            record.sortIndex);

    assertEquals("Customize Firefox", record.payload.get("title"));
    assertEquals("0-P9fabp9vJD",      record.payload.get("id"));
    assertTrue(record.payload.get("tags") instanceof JSONArray);
  }

  @Test
  public void testBaseCryptoRecordDecrypt() throws Exception {
    String base64CipherText =
          "NMsdnRulLwQsVcwxKW9XwaUe7ouJk5Wn"
        + "80QhbD80l0HEcZGCynh45qIbeYBik0lg"
        + "cHbKmlIxTJNwU+OeqipN+/j7MqhjKOGI"
        + "lvbpiPQQLC6/ffF2vbzL0nzMUuSyvaQz"
        + "yGGkSYM2xUFt06aNivoQTvU2GgGmUK6M"
        + "vadoY38hhW2LCMkoZcNfgCqJ26lO1O0s"
        + "EO6zHsk3IVz6vsKiJ2Hq6VCo7hu123wN"
        + "egmujHWQSGyf8JeudZjKzfi0OFRRvvm4"
        + "QAKyBWf0MgrW1F8SFDnVfkq8amCB7Nhd"
        + "whgLWbN+21NitNwWYknoEWe1m6hmGZDg"
        + "DT32uxzWxCV8QqqrpH/ZggViEr9uMgoy"
        + "4lYaWqP7G5WKvvechc62aqnsNEYhH26A"
        + "5QgzmlNyvB+KPFvPsYzxDnSCjOoRSLx7"
        + "GG86wT59QZw=";
    String base64IV = "GX8L37AAb2FZJMzIoXlX8w==";
    String base16Hmac = 
          "b1e6c18ac30deb70236bc0d65a46f7a4"
        + "dce3b8b0e02cf92182b914e3afa5eebc";

    ExtendedJSONObject body    = new ExtendedJSONObject();
    ExtendedJSONObject payload = new ExtendedJSONObject();
    payload.put("ciphertext", base64CipherText);
    payload.put("IV", base64IV);
    payload.put("hmac", base16Hmac);
    body.put("payload", payload.toJSONString());
    CryptoRecord record = CryptoRecord.fromJSONRecord(body);
    byte[] decodedKey  = Base64.decodeBase64(base64EncryptionKey.getBytes("UTF-8"));
    byte[] decodedHMAC = Base64.decodeBase64(base64HmacKey.getBytes("UTF-8")); 
    record.keyBundle = new KeyBundle(decodedKey, decodedHMAC);

    record.decrypt();
    String id = (String) record.payload.get("id");
    assertTrue(id.equals("5qRsgXWRJZXr"));
  }

  @Test
  public void testBaseCryptoRecordSyncKeyBundle() throws UnsupportedEncodingException, CryptoException {
    // These values pulled straight out of Firefox.
    String key  = "6m8mv8ex2brqnrmsb9fjuvfg7y";
    String user = "c6o7dvmr2c4ud2fyv6woz2u4zi22bcyd";
    
    // Check our friendly base32 decoding.
    assertTrue(Arrays.equals(Utils.decodeFriendlyBase32(key), Base64.decodeBase64("8xbKrJfQYwbFkguKmlSm/g==".getBytes("UTF-8"))));
    KeyBundle bundle = new KeyBundle(user, key);
    String expectedEncryptKeyBase64 = "/8RzbFT396htpZu5rwgIg2WKfyARgm7dLzsF5pwrVz8=";
    String expectedHMACKeyBase64    = "NChGjrqoXYyw8vIYP2334cvmMtsjAMUZNqFwV2LGNkM=";
    byte[] computedEncryptKey       = bundle.getEncryptionKey();
    byte[] computedHMACKey          = bundle.getHMACKey();
    assertTrue(Arrays.equals(computedEncryptKey, Base64.decodeBase64(expectedEncryptKeyBase64.getBytes("UTF-8"))));
    assertTrue(Arrays.equals(computedHMACKey,    Base64.decodeBase64(expectedHMACKeyBase64.getBytes("UTF-8"))));
  }

  @Test
  public void testDecrypt() throws Exception {
    String jsonInput =              "{\"sortindex\": 90, \"payload\":" +
                                    "\"{\\\"ciphertext\\\":\\\"F4ukf0" +
                                    "LM+vhffiKyjaANXeUhfmOPPmQYX1XBoG" +
                                    "Rh1LiHeKHB5rqjhzd7yAoxqgmFnkIgQF" +
                                    "YPSqRAoCxWiAeGULTX+KM4MU5drbNyR/" +
                                    "690JBWSyE1vQSiMGwNIbTKnOLGHKkQVY" +
                                    "HDpajg5BNFfvHNQ5Jx7uM9uJcmuEjCI6" +
                                    "GRMDKyKjhsTqCd99MONkY5rISutaWQ0e" +
                                    "EXFgpA9RZPv4jgWlQhe+YrVnpcrTi20b" +
                                    "NgKp3IfIeqEelrZ5FJd2WGZOA021d3e7" +
                                    "P3Z4qptefH4Q9/hySrWsELWngBaydyn/" +
                                    "IjsheZuKra3kJSST/4SvRZ7qXn\\\",\\" +
                                    "\"IV\\\":\\\"GadPajeXhpk75K2YH+L" +
                                    "y4w==\\\",\\\"hmac\\\":\\\"71442" +
                                    "d946502e3ca475c70a633d3d37f4b4e9" +
                                    "313a6d1041d0c0550cd354e7605\\\"}" +
                                    "\", \"id\": \"hkZYpC-BH4Xi\", \"" +
                                    "modified\": 1320183464.21}";
    String base64EncryptionKey =    "K8fV6PHG8RgugfHexGesbzTeOs2o12cr" +
                                    "N/G3bz0Bx1M=";
    String base64HmacKey =          "nbceuI6w1RJbBzh+iCJHEs8p4lElsOma" +
                                    "yUhx+OztVgM=";
    String expectedDecryptedText =  "{\"id\":\"hkZYpC-BH4Xi\",\"histU" +
                                    "ri\":\"http://hathology.com/2008" +
                                    "/06/how-to-edit-your-path-enviro" +
                                    "nment-variables-on-mac-os-x/\",\"" +
                                    "title\":\"How To Edit Your PATH " +
                                    "Environment Variables On Mac OS " +
                                    "X\",\"visits\":[{\"date\":131898" +
                                    "2074310889,\"type\":1}]}";

    KeyBundle keyBundle = KeyBundle.fromBase64EncodedKeys(base64EncryptionKey, base64HmacKey);

    CryptoRecord encrypted = CryptoRecord.fromJSONRecord(jsonInput);
    encrypted.keyBundle = keyBundle;
    CryptoRecord decrypted = encrypted.decrypt();

    // We don't necessarily produce exactly the same JSON but we do have the same values.
    ExtendedJSONObject expectedJson = new ExtendedJSONObject(expectedDecryptedText);
    assertEquals(expectedJson.get("id"), decrypted.payload.get("id"));
    assertEquals(expectedJson.get("title"), decrypted.payload.get("title"));
    assertEquals(expectedJson.get("histUri"), decrypted.payload.get("histUri"));
  }

  @Test
  public void testEncryptDecrypt() throws Exception {
      String originalText =           "{\"id\":\"hkZYpC-BH4Xi\",\"histU" +
                                      "ri\":\"http://hathology.com/2008" +
                                      "/06/how-to-edit-your-path-enviro" +
                                      "nment-variables-on-mac-os-x/\",\"" +
                                      "title\":\"How To Edit Your PATH " +
                                      "Environment Variables On Mac OS " +
                                      "X\",\"visits\":[{\"date\":131898" +
                                      "2074310889,\"type\":1}]}";
      String base64EncryptionKey =    "K8fV6PHG8RgugfHexGesbzTeOs2o12cr" +
                                      "N/G3bz0Bx1M=";
      String base64HmacKey =          "nbceuI6w1RJbBzh+iCJHEs8p4lElsOma" +
                                      "yUhx+OztVgM=";

      KeyBundle keyBundle = KeyBundle.fromBase64EncodedKeys(base64EncryptionKey, base64HmacKey);

      // Encrypt.
      CryptoRecord unencrypted = new CryptoRecord(originalText);
      unencrypted.keyBundle = keyBundle;
      CryptoRecord encrypted = unencrypted.encrypt();

      // Decrypt after round-trip through JSON.
      CryptoRecord undecrypted = CryptoRecord.fromJSONRecord(encrypted.toJSONString());
      undecrypted.keyBundle = keyBundle;
      CryptoRecord decrypted = undecrypted.decrypt();

      // We don't necessarily produce exactly the same JSON but we do have the same values.
      ExtendedJSONObject expectedJson = new ExtendedJSONObject(originalText);
      assertEquals(expectedJson.get("id"), decrypted.payload.get("id"));
      assertEquals(expectedJson.get("title"), decrypted.payload.get("title"));
      assertEquals(expectedJson.get("histUri"), decrypted.payload.get("histUri"));
  }

  @Test
  public void testDecryptKeysBundle() throws Exception {
    String jsonInput =                      "{\"payload\": \"{\\\"ciphertext\\" +
                                            "\":\\\"L1yRyZBkVYKXC1cTpeUqqfmKg" +
                                            "CinYV9YntGiG0PfYZSTLQ2s86WPI0VBb" +
                                            "QbLZfx7udk6sf6CFE4w5EgiPx0XP3Fbj" +
                                            "L7r4qIT0vjbAOrLKedZwA3cgiquc+PXM" +
                                            "Etml8B4Dfm0crJK0iROlRkb+lePAYkzI" +
                                            "iQn5Ba8mSWQEFoLy3zAcfCYXumA7E0Fj" +
                                            "XYD+TqTG5bqYJY4zvPaB9mn9y3WHw==\\" +
                                            "\",\\\"IV\\\":\\\"Jjb2oVI5uvvFfm" +
                                            "ZYRY4GaA==\\\",\\\"hmac\\\":\\\"" +
                                            "0b59731cb1aaedc85f54917b7058f361" +
                                            "60826b70050b0d70cd42b0b609b1d717" +
                                            "\\\"}\", \"id\": \"keys\", \"mod" +
                                            "ified\": 1320183463.91}";
    String username =                       "b6evr62dptbxz7fvebek7btljyu322wp";
    String friendlyBase32SyncKey =          "basuxv2426eqj7frhvpcwkavdi";
    String expectedDecryptedText =          "{\"default\":[\"K8fV6PHG8RgugfHe" +
                                            "xGesbzTeOs2o12crN/G3bz0Bx1M=\",\"" +
                                            "nbceuI6w1RJbBzh+iCJHEs8p4lElsOma" +
                                            "yUhx+OztVgM=\"],\"collections\":" +
                                            "{},\"collection\":\"crypto\",\"i" +
                                            "d\":\"keys\"}";
    String expectedBase64EncryptionKey =    "K8fV6PHG8RgugfHexGesbzTeOs2o12cr" +
                                            "N/G3bz0Bx1M=";
    String expectedBase64HmacKey =          "nbceuI6w1RJbBzh+iCJHEs8p4lElsOma" +
                                            "yUhx+OztVgM=";

    KeyBundle syncKeyBundle = new KeyBundle(username, friendlyBase32SyncKey);

    ExtendedJSONObject json = new ExtendedJSONObject(jsonInput);
    assertEquals("keys", json.get("id"));

    CryptoRecord encrypted = CryptoRecord.fromJSONRecord(jsonInput);
    encrypted.keyBundle = syncKeyBundle;
    CryptoRecord decrypted = encrypted.decrypt();

    // We don't necessarily produce exactly the same JSON but we do have the same values.
    ExtendedJSONObject expectedJson = new ExtendedJSONObject(expectedDecryptedText);
    assertEquals(expectedJson.get("id"), decrypted.payload.get("id"));
    assertEquals(expectedJson.get("default"), decrypted.payload.get("default"));
    assertEquals(expectedJson.get("collection"), decrypted.payload.get("collection"));
    assertEquals(expectedJson.get("collections"), decrypted.payload.get("collections"));

    // Check that the extracted keys were as expected.
    JSONArray keys = ExtendedJSONObject.parseJSONObject(decrypted.payload.toJSONString()).getArray("default");
    KeyBundle keyBundle = KeyBundle.fromBase64EncodedKeys((String)keys.get(0), (String)keys.get(1));

    assertArrayEquals(Base64.decodeBase64(expectedBase64EncryptionKey.getBytes("UTF-8")), keyBundle.getEncryptionKey());
    assertArrayEquals(Base64.decodeBase64(expectedBase64HmacKey.getBytes("UTF-8")), keyBundle.getHMACKey());
  }

  @Test
  public void testTTL() throws UnsupportedEncodingException, CryptoException {
    Record historyRecord = new HistoryRecord();
    CryptoRecord cryptoRecord = historyRecord.getEnvelope();
    assertEquals(historyRecord.ttl, cryptoRecord.ttl);

    // Very important that ttls are set in outbound envelopes.
    JSONObject o = cryptoRecord.toJSONObject();
    assertEquals(cryptoRecord.ttl, o.get("ttl"));

    // Most important of all, outbound encrypted record envelopes.
    KeyBundle keyBundle = KeyBundle.withRandomKeys();
    cryptoRecord.keyBundle = keyBundle;
    cryptoRecord.encrypt();
    assertEquals(historyRecord.ttl, cryptoRecord.ttl); // Should be preserved.
    o = cryptoRecord.toJSONObject();
    assertEquals(cryptoRecord.ttl, o.get("ttl"));

    // But we should ignore negative ttls.
    Record clientRecord = new ClientRecord();
    clientRecord.ttl = -1; // Don't ttl this record.
    o = clientRecord.getEnvelope().toJSONObject();
    assertNull(o.get("ttl"));

    // But we should ignore negative ttls in outbound encrypted record envelopes.
    cryptoRecord = clientRecord.getEnvelope();
    cryptoRecord.keyBundle = keyBundle;
    cryptoRecord.encrypt();
    o = cryptoRecord.toJSONObject();
    assertNull(o.get("ttl"));
  }
}
