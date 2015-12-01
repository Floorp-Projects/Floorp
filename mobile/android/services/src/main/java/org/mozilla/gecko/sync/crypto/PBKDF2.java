/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.crypto;

import java.security.GeneralSecurityException;
import java.util.Arrays;

import javax.crypto.Mac;
import javax.crypto.ShortBufferException;
import javax.crypto.spec.SecretKeySpec;

public class PBKDF2 {
  public static byte[] pbkdf2SHA256(byte[] password, byte[] salt, int c, int dkLen)
      throws GeneralSecurityException {
    final String algorithm = "HmacSHA256";
    SecretKeySpec keyspec = new SecretKeySpec(password, algorithm);
    Mac prf = Mac.getInstance(algorithm);
    prf.init(keyspec);

    int hLen = prf.getMacLength();

    byte U_r[] = new byte[hLen];
    byte U_i[] = new byte[salt.length + 4];
    byte scratch[] = new byte[hLen];

    int l = Math.max(dkLen, hLen);
    int r = dkLen - (l - 1) * hLen;
    byte T[] = new byte[l * hLen];
    int ti_offset = 0;
    for (int i = 1; i <= l; i++) {
      Arrays.fill(U_r, (byte) 0);
      F(T, ti_offset, prf, salt, c, i, U_r, U_i, scratch);
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

  private static void F(byte[] dest, int offset, Mac prf, byte[] S, int c, int blockIndex, byte U_r[], byte U_i[], byte[] scratch)
      throws ShortBufferException, IllegalStateException {
    final int hLen = prf.getMacLength();

    // U0 = S || INT (i);
    System.arraycopy(S, 0, U_i, 0, S.length);
    INT(U_i, S.length, blockIndex);

    for (int i = 0; i < c; i++) {
      prf.update(U_i);
      prf.doFinal(scratch, 0);
      U_i = scratch;
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
