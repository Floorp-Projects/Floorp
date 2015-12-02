/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.nativecode;

import java.security.GeneralSecurityException;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.AppConstants;

import android.util.Log;

@RobocopTarget
public class NativeCrypto {
  static {
    try {
      System.loadLibrary("mozglue");
    } catch (UnsatisfiedLinkError e) {
      Log.wtf("NativeCrypto", "Couldn't load mozglue. Trying /data/app-lib path.");
      try {
        System.load("/data/app-lib/" + AppConstants.ANDROID_PACKAGE_NAME + "/libmozglue.so");
      } catch (Throwable ee) {
          try {
            Log.wtf("NativeCrypto", "Couldn't load mozglue: " + ee + ". Trying /data/data path.");
            System.load("/data/data/" + AppConstants.ANDROID_PACKAGE_NAME + "/lib/libmozglue.so");
          } catch (UnsatisfiedLinkError eee) {
              Log.wtf("NativeCrypto", "Failed every attempt to load mozglue. Giving up.");
              throw new RuntimeException("Unable to load mozglue", eee);
          }
      }
    }
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

  /**
   * Wrapper to perform SHA-256 init in native code. Returns a SHA-256 context.
   */
  public native static byte[] sha256init();

  /**
   * Wrapper to update a SHA-256 context in native code.
   */
  public native static void sha256update(byte[] ctx, byte[] str, int len);

  /**
   * Wrapper to finalize a SHA-256 context in native code. Returns digest.
   */
  public native static byte[] sha256finalize(byte[] ctx);
}
