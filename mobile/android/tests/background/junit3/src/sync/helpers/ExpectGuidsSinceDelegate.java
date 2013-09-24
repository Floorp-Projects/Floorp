/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import junit.framework.AssertionFailedError;

public class ExpectGuidsSinceDelegate extends DefaultGuidsSinceDelegate {
  private String[] expected;
  public Set<String> ignore = new HashSet<String>();

  public ExpectGuidsSinceDelegate(String[] guids) {
    expected = guids;
    Arrays.sort(expected);
  }

  @Override
  public void onGuidsSinceSucceeded(String[] guids) {
    AssertionFailedError err = null;
    try {
      int notIgnored = 0;
      for (String guid : guids) {
        if (!ignore.contains(guid)) {
          notIgnored++;
          assertFalse(-1 == Arrays.binarySearch(this.expected, guid));
        }
      }
      assertEquals(this.expected.length, notIgnored);
    } catch (AssertionFailedError e) {
      err = e;
    }
    performNotify(err);
  }
}
