/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

import java.io.UnsupportedEncodingException;
import java.security.GeneralSecurityException;

import javax.crypto.Mac;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;
import javax.crypto.spec.SecretKeySpec;

public class PBKDF2 {
  public static byte[] pbkdf2SHA1(byte[] password, byte[] salt, int c, int dkLen)
      throws GeneralSecurityException {
    // Won't work on API level 8, but this is trivial.
    SecretKeyFactory factory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA1");
    PBEKeySpec keySpec;
    try {
      keySpec = new PBEKeySpec(new String(password, "UTF-8").toCharArray(), salt, c, dkLen * 8);
    } catch (UnsupportedEncodingException e) {
      throw new GeneralSecurityException(e);
    }
    SecretKey key = factory.generateSecret(keySpec);
    return key.getEncoded();
  }

  public static byte[] pbkdf2SHA256(byte[] password, byte[] salt, int c, int dkLen)
      throws GeneralSecurityException {
    final String algorithm = "HmacSHA256";
    SecretKeySpec keyspec = new SecretKeySpec(password, algorithm);
    Mac prf = Mac.getInstance(algorithm);
    prf.init(keyspec);

    int hLen = prf.getMacLength();
    int l = Math.max(dkLen, hLen);
    int r = dkLen - (l - 1) * hLen;
    byte T[] = new byte[l * hLen];
    int ti_offset = 0;
    for (int i = 1; i <= l; i++) {
      F(T, ti_offset, prf, salt, c, i);
      ti_offset += hLen;
    }

    if (r < hLen) {
      // Incomplete last block.
      byte DK[] = new byte[dkLen];
      System.arraycopy(T, 0, DK, 0, dkLen);
      return DK;
    }

    return T;
  }

  private static void F(byte[] dest, int offset, Mac prf, byte[] S, int c, int blockIndex) {
    final int hLen = prf.getMacLength();
    byte U_r[] = new byte[hLen];

    // U0 = S || INT (i);
    byte U_i[] = new byte[S.length + 4];
    System.arraycopy(S, 0, U_i, 0, S.length);
    INT(U_i, S.length, blockIndex);

    for (int i = 0; i < c; i++) {
      U_i = prf.doFinal(U_i);
      xor(U_r, U_i);
    }

    System.arraycopy(U_r, 0, dest, offset, hLen);
  }

  private static void xor(byte[] dest, byte[] src) {
    for (int i = 0; i < dest.length; i++) {
      dest[i] ^= src[i];
    }
  }

  private static void INT(byte[] dest, int offset, int i) {
    dest[offset + 0] = (byte) (i / (256 * 256 * 256));
    dest[offset + 1] = (byte) (i / (256 * 256));
    dest[offset + 2] = (byte) (i / (256));
    dest[offset + 3] = (byte) (i);
  }
}
