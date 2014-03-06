/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.nativecode;

import org.mozilla.gecko.mozglue.RobocopTarget;

import java.security.GeneralSecurityException;

@RobocopTarget
public class NativeCrypto {
  static {
    System.loadLibrary("mozglue");
  }

  /**
   * Wrapper to perform PBKDF2-HMAC-SHA-256 in native code.
   */
  public native static byte[] pbkdf2SHA256(byte[] password, byte[] salt, int c, int dkLen)
      throws GeneralSecurityException;

  /**
   * Wrapper to perform SHA-1 in native code.
   */
  public native static byte[] sha1(byte[] str);
}
