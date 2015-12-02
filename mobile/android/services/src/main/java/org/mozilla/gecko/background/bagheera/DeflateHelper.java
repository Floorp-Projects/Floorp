/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.bagheera;

import java.io.UnsupportedEncodingException;
import java.util.zip.Deflater;

import ch.boye.httpclientandroidlib.HttpEntity;

public class DeflateHelper {
  /**
   * Conservative upper bound for zlib size, equivalent to the first few lines
   * in zlib's deflateBound function.
   *
   * Includes zlib header.
   *
   * @param sourceLen
   *          the number of bytes to compress.
   * @return the number of bytes to allocate for the compressed output.
   */
  public static int deflateBound(final int sourceLen) {
    return sourceLen + ((sourceLen + 7) >> 3) + ((sourceLen + 63) >> 6) + 5 + 6;
  }

  /**
   * Deflate the input into the output array, returning the number of bytes
   * written to output.
   */
  public static int deflate(byte[] input, byte[] output) {
    final Deflater deflater = new Deflater();
    deflater.setInput(input);
    deflater.finish();

    final int length = deflater.deflate(output);
    deflater.end();
    return length;
  }

  /**
   * Deflate the input, returning an HttpEntity that offers an accurate window
   * on the output.
   *
   * Note that this method does not trim the output array. (Test code can use
   * TestDeflation#deflateTrimmed(byte[]).)
   *
   * Trimming would be more efficient for long-term space use, but we expect this
   * entity to be transient.
   *
   * Note also that deflate can require <b>more</b> space than the input.
   * {@link #deflateBound(int)} tells us the most it will use.
   *
   * @param bytes the input to deflate.
   * @return the deflated input as an entity.
   */
  public static HttpEntity deflateBytes(final byte[] bytes) {
    // We would like to use DeflaterInputStream here, but it's minSDK=9, and we
    // still target 8. It would also force us to use chunked Transfer-Encoding,
    // so perhaps it's for the best!

    final byte[] out = new byte[deflateBound(bytes.length)];
    final int outLength = deflate(bytes, out);
    return new BoundedByteArrayEntity(out, 0, outLength);
  }

  public static HttpEntity deflateBody(final String payload) {
    final byte[] bytes;
    try {
      bytes = payload.getBytes("UTF-8");
    } catch (UnsupportedEncodingException ex) {
      // This will never happen. Thanks, Java!
      throw new RuntimeException(ex);
    }
    return deflateBytes(bytes);
  }
}