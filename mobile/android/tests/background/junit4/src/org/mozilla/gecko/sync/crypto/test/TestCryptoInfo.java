/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.crypto.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import java.io.UnsupportedEncodingException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.apache.commons.codec.binary.Base64;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.CryptoInfo;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestCryptoInfo {

  @Test
  public void testEncryptedHMACIsSet() throws CryptoException, UnsupportedEncodingException, InvalidKeyException, NoSuchAlgorithmException {
    KeyBundle kb = KeyBundle.withRandomKeys();
    CryptoInfo encrypted = CryptoInfo.encrypt("plaintext".getBytes("UTF-8"), kb);
    assertSame(kb, encrypted.getKeys());
    assertTrue(encrypted.generatedHMACIsHMAC());
  }

  @Test
  public void testRandomEncryptedDecrypted() throws CryptoException, UnsupportedEncodingException, InvalidKeyException, NoSuchAlgorithmException {
    KeyBundle kb = KeyBundle.withRandomKeys();
    byte[] plaintext = "plaintext".getBytes("UTF-8");
    CryptoInfo info = CryptoInfo.encrypt(plaintext, kb);
    byte[] iv = info.getIV();
    info.decrypt();
    assertArrayEquals(plaintext, info.getMessage());
    assertSame(null, info.getHMAC());
    assertArrayEquals(iv, info.getIV());
    assertSame(kb, info.getKeys());
  }

  @Test
  public void testDecrypt() throws CryptoException {
    String base64CipherText =    "NMsdnRulLwQsVcwxKW9XwaUe7ouJk5Wn" +
                                 "80QhbD80l0HEcZGCynh45qIbeYBik0lg" +
                                 "cHbKmlIxTJNwU+OeqipN+/j7MqhjKOGI" +
                                 "lvbpiPQQLC6/ffF2vbzL0nzMUuSyvaQz" +
                                 "yGGkSYM2xUFt06aNivoQTvU2GgGmUK6M" +
                                 "vadoY38hhW2LCMkoZcNfgCqJ26lO1O0s" +
                                 "EO6zHsk3IVz6vsKiJ2Hq6VCo7hu123wN" +
                                 "egmujHWQSGyf8JeudZjKzfi0OFRRvvm4" +
                                 "QAKyBWf0MgrW1F8SFDnVfkq8amCB7Nhd" +
                                 "whgLWbN+21NitNwWYknoEWe1m6hmGZDg" +
                                 "DT32uxzWxCV8QqqrpH/ZggViEr9uMgoy" +
                                 "4lYaWqP7G5WKvvechc62aqnsNEYhH26A" +
                                 "5QgzmlNyvB+KPFvPsYzxDnSCjOoRSLx7" +
                                 "GG86wT59QZw=";
    String base64IV =            "GX8L37AAb2FZJMzIoXlX8w==";
    String base16Hmac =          "b1e6c18ac30deb70236bc0d65a46f7a4" +
                                 "dce3b8b0e02cf92182b914e3afa5eebc";
    String base64EncryptionKey = "9K/wLdXdw+nrTtXo4ZpECyHFNr4d7aYH" +
                                 "qeg3KW9+m6Q=";
    String base64HmacKey =       "MMntEfutgLTc8FlTLQFms8/xMPmCldqP" +
                                 "lq/QQXEjx70=";
    String base64ExpectedBytes = "eyJpZCI6IjVxUnNnWFdSSlpYciIsImhp" +
                                 "c3RVcmkiOiJmaWxlOi8vL1VzZXJzL2ph" +
                                 "c29uL0xpYnJhcnkvQXBwbGljYXRpb24l" +
                                 "MjBTdXBwb3J0L0ZpcmVmb3gvUHJvZmls" +
                                 "ZXMva3NnZDd3cGsuTG9jYWxTeW5jU2Vy" +
                                 "dmVyL3dlYXZlL2xvZ3MvIiwidGl0bGUi" +
                                 "OiJJbmRleCBvZiBmaWxlOi8vL1VzZXJz" +
                                 "L2phc29uL0xpYnJhcnkvQXBwbGljYXRp" +
                                 "b24gU3VwcG9ydC9GaXJlZm94L1Byb2Zp" +
                                 "bGVzL2tzZ2Q3d3BrLkxvY2FsU3luY1Nl" +
                                 "cnZlci93ZWF2ZS9sb2dzLyIsInZpc2l0" +
                                 "cyI6W3siZGF0ZSI6MTMxOTE0OTAxMjM3" +
                                 "MjQyNSwidHlwZSI6MX1dfQ==";

    CryptoInfo decrypted = CryptoInfo.decrypt(
            Base64.decodeBase64(base64CipherText),
            Base64.decodeBase64(base64IV),
            Utils.hex2Byte(base16Hmac),
            new KeyBundle(
                Base64.decodeBase64(base64EncryptionKey),
                Base64.decodeBase64(base64HmacKey))
            );

    assertArrayEquals(decrypted.getMessage(), Base64.decodeBase64(base64ExpectedBytes));
  }

  @Test
  public void testEncrypt() throws CryptoException {
    String base64CipherText =    "NMsdnRulLwQsVcwxKW9XwaUe7ouJk5Wn" +
                                 "80QhbD80l0HEcZGCynh45qIbeYBik0lg" +
                                 "cHbKmlIxTJNwU+OeqipN+/j7MqhjKOGI" +
                                 "lvbpiPQQLC6/ffF2vbzL0nzMUuSyvaQz" +
                                 "yGGkSYM2xUFt06aNivoQTvU2GgGmUK6M" +
                                 "vadoY38hhW2LCMkoZcNfgCqJ26lO1O0s" +
                                 "EO6zHsk3IVz6vsKiJ2Hq6VCo7hu123wN" +
                                 "egmujHWQSGyf8JeudZjKzfi0OFRRvvm4" +
                                 "QAKyBWf0MgrW1F8SFDnVfkq8amCB7Nhd" +
                                 "whgLWbN+21NitNwWYknoEWe1m6hmGZDg" +
                                 "DT32uxzWxCV8QqqrpH/ZggViEr9uMgoy" +
                                 "4lYaWqP7G5WKvvechc62aqnsNEYhH26A" +
                                 "5QgzmlNyvB+KPFvPsYzxDnSCjOoRSLx7" +
                                 "GG86wT59QZw=";
    String base64IV =            "GX8L37AAb2FZJMzIoXlX8w==";
    String base16Hmac =          "b1e6c18ac30deb70236bc0d65a46f7a4" +
                                 "dce3b8b0e02cf92182b914e3afa5eebc";
    String base64EncryptionKey = "9K/wLdXdw+nrTtXo4ZpECyHFNr4d7aYH" +
                                 "qeg3KW9+m6Q=";
    String base64HmacKey =       "MMntEfutgLTc8FlTLQFms8/xMPmCldqP" +
                                 "lq/QQXEjx70=";
    String base64ExpectedBytes = "eyJpZCI6IjVxUnNnWFdSSlpYciIsImhp" +
                                 "c3RVcmkiOiJmaWxlOi8vL1VzZXJzL2ph" +
                                 "c29uL0xpYnJhcnkvQXBwbGljYXRpb24l" +
                                 "MjBTdXBwb3J0L0ZpcmVmb3gvUHJvZmls" +
                                 "ZXMva3NnZDd3cGsuTG9jYWxTeW5jU2Vy" +
                                 "dmVyL3dlYXZlL2xvZ3MvIiwidGl0bGUi" +
                                 "OiJJbmRleCBvZiBmaWxlOi8vL1VzZXJz" +
                                 "L2phc29uL0xpYnJhcnkvQXBwbGljYXRp" +
                                 "b24gU3VwcG9ydC9GaXJlZm94L1Byb2Zp" +
                                 "bGVzL2tzZ2Q3d3BrLkxvY2FsU3luY1Nl" +
                                 "cnZlci93ZWF2ZS9sb2dzLyIsInZpc2l0" +
                                 "cyI6W3siZGF0ZSI6MTMxOTE0OTAxMjM3" +
                                 "MjQyNSwidHlwZSI6MX1dfQ==";

    CryptoInfo encrypted = CryptoInfo.encrypt(
            Base64.decodeBase64(base64ExpectedBytes),
            Base64.decodeBase64(base64IV),
            new KeyBundle(
                Base64.decodeBase64(base64EncryptionKey),
                Base64.decodeBase64(base64HmacKey))
            );

    assertArrayEquals(Base64.decodeBase64(base64CipherText), encrypted.getMessage());
    assertArrayEquals(Utils.hex2Byte(base16Hmac), encrypted.getHMAC());
  }
}