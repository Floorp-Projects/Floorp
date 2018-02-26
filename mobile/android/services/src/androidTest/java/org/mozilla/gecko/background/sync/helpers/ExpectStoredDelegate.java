/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertEquals;
import junit.framework.AssertionFailedError;

public class ExpectStoredDelegate extends DefaultStoreDelegate {
  final int expectedCount;
  int count;
  public ExpectStoredDelegate(int expectedCount) {
    this.expectedCount = expectedCount;
  }

  @Override
  public synchronized void onStoreCompleted() {
    try {
      assertEquals(expectedCount, count);
      performNotify();
    } catch (AssertionFailedError e) {
      performNotify("Wrong # of GUIDS stored: " + count, e);
    }
  }

  @Override
  public synchronized void onRecordStoreSucceeded(int count) {
    this.count += count;
  }
}
