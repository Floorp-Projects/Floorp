/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.util.Log;

public class Utils {
  public static long getThreadId() {
    final Thread t = Thread.currentThread();
    return t.getId();
  }

  public static String getThreadSignature() {
    final Thread t = Thread.currentThread();
    final long l = t.getId();
    final String name = t.getName();
    final long p = t.getPriority();
    final String gname = t.getThreadGroup().getName();
    return (name + ":(id)" + l + ":(priority)" + p + ":(group)" + gname);
  }

  public static void logThreadSignature() {
    Log.d("ThreadUtils", getThreadSignature());
  }

  private static final char[] hexArray = "0123456789ABCDEF".toCharArray();

  public static String bytesToHex(final byte[] bytes) {
    final char[] hexChars = new char[bytes.length * 2];
    for (int j = 0; j < bytes.length; j++) {
      final int v = bytes[j] & 0xFF;
      hexChars[j * 2] = hexArray[v >>> 4];
      hexChars[j * 2 + 1] = hexArray[v & 0x0F];
    }
    return new String(hexChars);
  }
}
