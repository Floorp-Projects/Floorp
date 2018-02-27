/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import java.util.HashSet;
import java.util.concurrent.atomic.AtomicLong;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.sync.repositories.domain.Record;

public class ExpectManyStoredDelegate extends DefaultStoreDelegate {
  int expectedStored;
  AtomicLong stored;

  public ExpectManyStoredDelegate(Record[] records) {
    expectedStored = records.length;
    stored = new AtomicLong(0);
  }

  @Override
  public void onStoreCompleted() {
    try {
      assertEquals(expectedStored, stored.get());
      performNotify();
    } catch (AssertionFailedError e) {
      performNotify(e);
    }
  }

  @Override
  public void onRecordStoreSucceeded(int count) {
    stored.addAndGet(count);
  }
}
