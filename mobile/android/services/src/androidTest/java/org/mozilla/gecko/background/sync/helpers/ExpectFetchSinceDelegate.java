/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import java.util.Arrays;

import org.mozilla.gecko.sync.repositories.domain.Record;

import junit.framework.AssertionFailedError;

public class ExpectFetchSinceDelegate extends DefaultFetchDelegate {
  private String[] expected;
  private long earliest;

  public ExpectFetchSinceDelegate(long timestamp, String[] guids) {
    expected = guids;
    earliest = timestamp;
    Arrays.sort(expected);
  }

  @Override
  public void onFetchCompleted() {
    AssertionFailedError err = null;
    try {
      int countSpecials = 0;
      for (Record record : records) {
        // Check if record should be ignored.
        if (!ignore.contains(record.guid)) {
          assertFalse(-1 == Arrays.binarySearch(this.expected, record.guid));
        } else {
          countSpecials++;
        }
        // Check that record is later than timestamp-earliest.
        assertTrue(record.lastModified >= this.earliest);
      }
      assertEquals(this.expected.length, records.size() - countSpecials);
    } catch (AssertionFailedError e) {
      err = e;
    }
    performNotify(err);
  }
}
