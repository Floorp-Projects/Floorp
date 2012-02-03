/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

/*
 * All info in these objects should be decoded (i.e. not BaseXX encoded).
 */
public class CryptoInfo {

  private byte[] message;
  private byte[] iv;
  private byte[] hmac;
  private KeyBundle keys;

  /*
   * Constructor typically used when encrypting.
   */
  public CryptoInfo(byte[] message, KeyBundle keys) {
    this.setMessage(message);
    this.setKeys(keys);
  }

  /*
   * Constructor typically used when decrypting.
   */
  public CryptoInfo(byte[] message, byte[] iv, byte[] hmac, KeyBundle keys) {
    this.setMessage(message);
    this.setIV(iv);
    this.setHMAC(hmac);
    this.setKeys(keys);
  }

  public byte[] getMessage() {
    return message;
  }

  public void setMessage(byte[] message) {
    this.message = message;
  }

  public byte[] getIV() {
    return iv;
  }

  public void setIV(byte[] iv) {
    this.iv = iv;
  }

  public byte[] getHMAC() {
    return hmac;
  }

  public void setHMAC(byte[] hmac) {
    this.hmac = hmac;
  }

  public KeyBundle getKeys() {
    return keys;
  }

  public void setKeys(KeyBundle keys) {
    this.keys = keys;
  }
}
