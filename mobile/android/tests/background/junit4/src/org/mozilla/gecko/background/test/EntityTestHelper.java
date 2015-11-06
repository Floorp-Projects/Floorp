/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.test;

import ch.boye.httpclientandroidlib.HttpEntity;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class EntityTestHelper {
  private static final int DEFAULT_SIZE = 1024;

  public static byte[] bytesFromEntity(final HttpEntity entity) throws IOException {
    final InputStream is = entity.getContent();

    if (is instanceof ByteArrayInputStream) {
      final int size = is.available();
      final byte[] buffer = new byte[size];
      is.read(buffer, 0, size);
      return buffer;
    }

    final ByteArrayOutputStream bos = new ByteArrayOutputStream();
    final byte[] buffer = new byte[DEFAULT_SIZE];
    int len;
    while ((len = is.read(buffer, 0, DEFAULT_SIZE)) != -1) {
      bos.write(buffer, 0, len);
    }
    return bos.toByteArray();
  }
}