/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.fail;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutorService;

import junit.framework.AssertionFailedError;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

public class DefaultFetchDelegate extends DefaultDelegate implements RepositorySessionFetchRecordsDelegate {

  private static final String LOG_TAG = "DefaultFetchDelegate";
  public ArrayList<Record> records = new ArrayList<Record>();
  public Set<String> ignore = new HashSet<String>();

  @Override
  public void onFetchFailed(Exception ex) {
    performNotify("Fetch failed.", ex);
  }

  protected void onDone(ArrayList<Record> records, HashMap<String, Record> expected, long end) {
    Logger.debug(LOG_TAG, "onDone.");
    Logger.debug(LOG_TAG, "End timestamp is " + end);
    Logger.debug(LOG_TAG, "Expected is " + expected);
    Logger.debug(LOG_TAG, "Records is " + records);
    Set<String> foundGuids = new HashSet<String>();
    try {
      int expectedCount = 0;
      int expectedFound = 0;
      Logger.debug(LOG_TAG, "Counting expected keys.");
      for (String key : expected.keySet()) {
        if (!ignore.contains(key)) {
          expectedCount++;
        }
      }
      Logger.debug(LOG_TAG, "Expected keys: " + expectedCount);
      for (Record record : records) {
        Logger.debug(LOG_TAG, "Record.");
        Logger.debug(LOG_TAG, record.guid);

        // Ignore special GUIDs (e.g., for bookmarks).
        if (!ignore.contains(record.guid)) {
          if (foundGuids.contains(record.guid)) {
            fail("Found duplicate guid " + record.guid);
          }
          Record expect = expected.get(record.guid);
          if (expect == null) {
            fail("Do not expect to get back a record with guid: " + record.guid); // Caught below
          }
          Logger.debug(LOG_TAG, "Checking equality.");
          try {
            assertTrue(expect.equalPayloads(record)); // Caught below
          } catch (Exception e) {
            Logger.error(LOG_TAG, "ONOZ!", e);
          }
          Logger.debug(LOG_TAG, "Checked equality.");
          expectedFound += 1;
          // Track record once we've found it.
          foundGuids.add(record.guid);
        }
      }
      assertEquals(expectedCount, expectedFound); // Caught below
      Logger.debug(LOG_TAG, "Notifying success.");
      performNotify();
    } catch (AssertionFailedError e) {
      Logger.error(LOG_TAG, "Notifying assertion failure.");
      performNotify(e);
    } catch (Exception e) {
      Logger.error(LOG_TAG, "No!");
      performNotify();
    }
  }

  public int recordCount() {
    return (this.records == null) ? 0 : this.records.size();
  }

  @Override
  public void onFetchedRecord(Record record) {
    Logger.debug(LOG_TAG, "onFetchedRecord(" + record.guid + ")");
    records.add(record);
  }

  @Override
  public void onFetchCompleted(final long fetchEnd) {
    Logger.debug(LOG_TAG, "onFetchCompleted. Doing nothing.");
  }

  @Override
  public void onBatchCompleted() {
    Logger.debug(LOG_TAG, "onBatchCompleted. Doing nothing.");
  }

  @Override
  public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(final ExecutorService executor) {
    return new DeferredRepositorySessionFetchRecordsDelegate(this, executor);
  }
}
