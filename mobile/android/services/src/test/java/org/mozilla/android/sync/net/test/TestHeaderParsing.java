/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.Utils;

import static org.junit.Assert.assertEquals;

@RunWith(TestRunner.class)
public class TestHeaderParsing {

  @SuppressWarnings("static-method")
  @Test
  public void testDecimalSecondsToMilliseconds() {
    assertEquals(Utils.decimalSecondsToMilliseconds(""),         -1);
    assertEquals(Utils.decimalSecondsToMilliseconds("1234.1.1"), -1);
    assertEquals(Utils.decimalSecondsToMilliseconds("1234"),     1234000);
    assertEquals(Utils.decimalSecondsToMilliseconds("1234.123"), 1234123);
    assertEquals(Utils.decimalSecondsToMilliseconds("1234.12"),  1234120);

    assertEquals("1234.000", Utils.millisecondsToDecimalSecondsString(1234000));
    assertEquals("1234.123", Utils.millisecondsToDecimalSecondsString(1234123));
    assertEquals("1234.120", Utils.millisecondsToDecimalSecondsString(1234120));
  }
}
