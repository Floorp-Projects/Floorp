/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.test;

import java.io.IOException;
import java.util.Arrays;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.bagheera.BoundedByteArrayEntity;

import ch.boye.httpclientandroidlib.HttpEntity;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestBoundedByteArrayEntity {
  private static void expectFail(byte[] input, int start, int end) {
    try {
      new BoundedByteArrayEntity(input, start, end);
      Assert.fail("Should have thrown.");
    } catch (Exception ex) {
      return;
    }
  }

  private static void expectEmpty(byte[] source, int start, int end) {
    final HttpEntity entity = new BoundedByteArrayEntity(source, start, end);
    Assert.assertEquals(0, entity.getContentLength());
  }

  private static void expectSubArray(byte[] source, int start, int end) throws IOException {
    final HttpEntity entity = new BoundedByteArrayEntity(source, start, end);
    int expectedLength = end - start;
    byte[] expectedContents = Arrays.copyOfRange(source, start, end);
    Assert.assertEquals(expectedLength, entity.getContentLength());
    Assert.assertArrayEquals(expectedContents, EntityTestHelper.bytesFromEntity(entity));
  }

  @Test
  public final void testFail() {
    byte[] input = new byte[] { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6 };
    expectFail(input, -1, 3);
    expectFail(input, 3, 2);
    expectFail(input, 0, input.length + 1);
    expectFail(input, input.length + 1, input.length + 2);
  }

  @Test
  public final void testBounds() throws IOException {
    byte[] input = new byte[] { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6 };
    expectEmpty(input, 0, 0);
    expectEmpty(input, 3, 3);
    expectEmpty(input, input.length, input.length);
    expectSubArray(input, 0, 3);
    expectSubArray(input, 3, input.length);
    expectSubArray(input, 0, input.length);
  }
}
