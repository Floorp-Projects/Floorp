/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.db;

import java.util.ArrayList;

import org.json.simple.JSONObject;
import org.mozilla.gecko.background.sync.helpers.ExpectFetchDelegate;
import org.mozilla.gecko.background.sync.helpers.HistoryHelpers;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryDataAccessor;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryRepository;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserHistoryRepositorySession;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserRepository;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserRepositoryDataAccessor;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

public class TestAndroidBrowserHistoryRepository extends AndroidBrowserRepositoryTestCase {

  @Override
  protected AndroidBrowserRepository getRepository() {

    /**
     * Override this chain in order to avoid our test code having to create two
     * sessions all the time.
     */
    return new AndroidBrowserHistoryRepository() {
      @Override
      protected void sessionCreator(RepositorySessionCreationDelegate delegate, Context context) {
        AndroidBrowserHistoryRepositorySession session;
        session = new AndroidBrowserHistoryRepositorySession(this, context) {
          @Override
          protected synchronized void trackGUID(String guid) {
            System.out.println("Ignoring trackGUID call: this is a test!");
          }
        };
        delegate.onSessionCreated(session);
      }
    };
  }

  @Override
  protected AndroidBrowserRepositoryDataAccessor getDataAccessor() {
    return new AndroidBrowserHistoryDataAccessor(getApplicationContext());
  }

  @Override
  public void testFetchAll() {
    Record[] expected = new Record[2];
    expected[0] = HistoryHelpers.createHistory3();
    expected[1] = HistoryHelpers.createHistory2();
    basicFetchAllTest(expected);
  }

  /*
   * Test storing identical records with different guids.
   * For bookmarks identical is defined by the following fields
   * being the same: title, uri, type, parentName
   */
  @Override
  public void testStoreIdenticalExceptGuid() {
    storeIdenticalExceptGuid(HistoryHelpers.createHistory1());
  }

  @Override
  public void testCleanMultipleRecords() {
    cleanMultipleRecords(
        HistoryHelpers.createHistory1(),
        HistoryHelpers.createHistory2(),
        HistoryHelpers.createHistory3(),
        HistoryHelpers.createHistory4(),
        HistoryHelpers.createHistory5()
        );
  }

  @Override
  public void testGuidsSinceReturnMultipleRecords() {
    HistoryRecord record0 = HistoryHelpers.createHistory1();
    HistoryRecord record1 = HistoryHelpers.createHistory2();
    guidsSinceReturnMultipleRecords(record0, record1);
  }

  @Override
  public void testGuidsSinceReturnNoRecords() {
    guidsSinceReturnNoRecords(HistoryHelpers.createHistory3());
  }

  @Override
  public void testFetchSinceOneRecord() {
    fetchSinceOneRecord(HistoryHelpers.createHistory1(),
        HistoryHelpers.createHistory2());
  }

  @Override
  public void testFetchSinceReturnNoRecords() {
    fetchSinceReturnNoRecords(HistoryHelpers.createHistory3());
  }

  @Override
  public void testFetchOneRecordByGuid() {
    fetchOneRecordByGuid(HistoryHelpers.createHistory1(),
        HistoryHelpers.createHistory2());
  }

  @Override
  public void testFetchMultipleRecordsByGuids() {
    HistoryRecord record0 = HistoryHelpers.createHistory1();
    HistoryRecord record1 = HistoryHelpers.createHistory2();
    HistoryRecord record2 = HistoryHelpers.createHistory3();
    fetchMultipleRecordsByGuids(record0, record1, record2);
  }

  @Override
  public void testFetchNoRecordByGuid() {
    fetchNoRecordByGuid(HistoryHelpers.createHistory1());
  }

  @Override
  public void testWipe() {
    doWipe(HistoryHelpers.createHistory2(), HistoryHelpers.createHistory3());
  }

  @Override
  public void testStore() {
    basicStoreTest(HistoryHelpers.createHistory1());
  }

  @Override
  public void testRemoteNewerTimeStamp() {
    HistoryRecord local = HistoryHelpers.createHistory1();
    HistoryRecord remote = HistoryHelpers.createHistory2();
    remoteNewerTimeStamp(local, remote);
  }

  @Override
  public void testLocalNewerTimeStamp() {
    HistoryRecord local = HistoryHelpers.createHistory1();
    HistoryRecord remote = HistoryHelpers.createHistory2();
    localNewerTimeStamp(local, remote);
  }

  @Override
  public void testDeleteRemoteNewer() {
    HistoryRecord local = HistoryHelpers.createHistory1();
    HistoryRecord remote = HistoryHelpers.createHistory2();
    deleteRemoteNewer(local, remote);
  }

  @Override
  public void testDeleteLocalNewer() {
    HistoryRecord local = HistoryHelpers.createHistory1();
    HistoryRecord remote = HistoryHelpers.createHistory2();
    deleteLocalNewer(local, remote);
  }

  @Override
  public void testDeleteRemoteLocalNonexistent() {
    deleteRemoteLocalNonexistent(HistoryHelpers.createHistory2());
  }

  /**
   * Exists to provide access to record string logic.
   */
  protected class HelperHistorySession extends AndroidBrowserHistoryRepositorySession {
    public HelperHistorySession(Repository repository, Context context) {
      super(repository, context);
    }

    public boolean sameRecordString(HistoryRecord r1, HistoryRecord r2) {
      return buildRecordString(r1).equals(buildRecordString(r2));
    }
  }

  /**
   * Verifies that two history records with the same URI but different
   * titles will be reconciled locally.
   */
  public void testRecordStringCollisionAndEquality() {
    final AndroidBrowserHistoryRepository repo = new AndroidBrowserHistoryRepository();
    final HelperHistorySession testSession = new HelperHistorySession(repo, getApplicationContext());

    final long now = RepositorySession.now();

    final HistoryRecord record0 = new HistoryRecord(null, "history", now + 1, false);
    final HistoryRecord record1 = new HistoryRecord(null, "history", now + 2, false);
    final HistoryRecord record2 = new HistoryRecord(null, "history", now + 3, false);

    record0.histURI = "http://example.com/foo";
    record1.histURI = "http://example.com/foo";
    record2.histURI = "http://example.com/bar";
    record0.title = "Foo 0";
    record1.title = "Foo 1";
    record2.title = "Foo 2";

    // Ensure that two records with the same URI produce the same record string,
    // and two records with different URIs do not.
    assertTrue(testSession.sameRecordString(record0, record1));
    assertFalse(testSession.sameRecordString(record0, record2));

    // Two records are congruent if they have the same URI and their
    // identifiers match (which is why these all have null GUIDs).
    assertTrue(record0.congruentWith(record0));
    assertTrue(record0.congruentWith(record1));
    assertTrue(record1.congruentWith(record0));
    assertFalse(record0.congruentWith(record2));
    assertFalse(record1.congruentWith(record2));
    assertFalse(record2.congruentWith(record1));
    assertFalse(record2.congruentWith(record0));

    // None of these records are equal, because they have different titles.
    // (Except for being equal to themselves, of course.)
    assertTrue(record0.equalPayloads(record0));
    assertTrue(record1.equalPayloads(record1));
    assertTrue(record2.equalPayloads(record2));
    assertFalse(record0.equalPayloads(record1));
    assertFalse(record1.equalPayloads(record0));
    assertFalse(record1.equalPayloads(record2));
  }

  /*
   * Tests for adding some visits to a history record
   * and doing a fetch.
   */
  @SuppressWarnings("unchecked")
  public void testAddOneVisit() {
    final RepositorySession session = createAndBeginSession();

    HistoryRecord record0 = HistoryHelpers.createHistory3();
    performWait(storeRunnable(session, record0));

    // Add one visit to the count and put in a new
    // last visited date.
    ContentValues cv = new ContentValues();
    int visits = record0.visits.size() + 1;
    long newVisitTime = System.currentTimeMillis();
    cv.put(BrowserContract.History.VISITS, visits);
    cv.put(BrowserContract.History.DATE_LAST_VISITED, newVisitTime);
    final AndroidBrowserRepositoryDataAccessor dataAccessor = getDataAccessor();
    dataAccessor.updateByGuid(record0.guid, cv);

    // Add expected visit to record for verification.
    JSONObject expectedVisit = new JSONObject();
    expectedVisit.put("date", newVisitTime * 1000);    // Microseconds.
    expectedVisit.put("type", 1L);
    record0.visits.add(expectedVisit);

    performWait(fetchRunnable(session, new String[] { record0.guid }, new ExpectFetchDelegate(new Record[] { record0 })));
    closeDataAccessor(dataAccessor);
  }

  @SuppressWarnings("unchecked")
  public void testAddMultipleVisits() {
    final RepositorySession session = createAndBeginSession();

    HistoryRecord record0 = HistoryHelpers.createHistory4();
    performWait(storeRunnable(session, record0));

    // Add three visits to the count and put in a new
    // last visited date.
    ContentValues cv = new ContentValues();
    int visits = record0.visits.size() + 3;
    long newVisitTime = System.currentTimeMillis();
    cv.put(BrowserContract.History.VISITS, visits);
    cv.put(BrowserContract.History.DATE_LAST_VISITED, newVisitTime);
    final AndroidBrowserRepositoryDataAccessor dataAccessor = getDataAccessor();
    dataAccessor.updateByGuid(record0.guid, cv);

    // Now shift to microsecond timing for visits.
    long newMicroVisitTime = newVisitTime * 1000;

    // Add expected visits to record for verification
    JSONObject expectedVisit = new JSONObject();
    expectedVisit.put("date", newMicroVisitTime);
    expectedVisit.put("type", 1L);
    record0.visits.add(expectedVisit);
    expectedVisit = new JSONObject();
    expectedVisit.put("date", newMicroVisitTime - 1000);
    expectedVisit.put("type", 1L);
    record0.visits.add(expectedVisit);
    expectedVisit = new JSONObject();
    expectedVisit.put("date", newMicroVisitTime - 2000);
    expectedVisit.put("type", 1L);
    record0.visits.add(expectedVisit);

    ExpectFetchDelegate delegate = new ExpectFetchDelegate(new Record[] { record0 });
    performWait(fetchRunnable(session, new String[] { record0.guid }, delegate));

    Record fetched = delegate.records.get(0);
    assertTrue(record0.equalPayloads(fetched));
    closeDataAccessor(dataAccessor);
  }

  public void testInvalidHistoryItemIsSkipped() throws NullCursorException {
    final AndroidBrowserHistoryRepositorySession session = (AndroidBrowserHistoryRepositorySession) createAndBeginSession();
    final AndroidBrowserRepositoryDataAccessor dbHelper = session.getDBHelper();

    final long now = System.currentTimeMillis();
    final HistoryRecord emptyURL = new HistoryRecord(Utils.generateGuid(), "history", now, false);
    final HistoryRecord noVisits = new HistoryRecord(Utils.generateGuid(), "history", now, false);
    final HistoryRecord aboutURL = new HistoryRecord(Utils.generateGuid(), "history", now, false);

    emptyURL.fennecDateVisited = now;
    emptyURL.fennecVisitCount  = 1;
    emptyURL.histURI           = "";
    emptyURL.title             = "Something";

    noVisits.fennecDateVisited = now;
    noVisits.fennecVisitCount  = 0;
    noVisits.histURI           = "http://example.org/novisits";
    noVisits.title             = "Something Else";

    aboutURL.fennecDateVisited = now;
    aboutURL.fennecVisitCount  = 1;
    aboutURL.histURI           = "about:home";
    aboutURL.title             = "Fennec Home";

    Uri one = dbHelper.insert(emptyURL);
    Uri two = dbHelper.insert(noVisits);
    Uri tre = dbHelper.insert(aboutURL);
    assertNotNull(one);
    assertNotNull(two);
    assertNotNull(tre);

    // The records are in the DB.
    final Cursor all = dbHelper.fetchAll();
    assertEquals(3, all.getCount());
    all.close();

    // But aren't returned by fetching.
    performWait(fetchAllRunnable(session, new Record[] {}));

    // And we'd ignore about:home if we downloaded it.
    assertTrue(session.shouldIgnore(aboutURL));

    session.abort();
  }

  public void testSqlInjectPurgeDelete() {
    // Some setup.
    RepositorySession session = createAndBeginSession();
    final AndroidBrowserRepositoryDataAccessor db = getDataAccessor();

    try {
      ContentValues cv = new ContentValues();
      cv.put(BrowserContract.SyncColumns.IS_DELETED, 1);

      // Create and insert 2 history entries, 2nd one is evil (attempts injection).
      HistoryRecord h1 = HistoryHelpers.createHistory1();
      HistoryRecord h2 = HistoryHelpers.createHistory2();
      h2.guid = "' or '1'='1";

      db.insert(h1);
      db.insert(h2);

      // Test 1 - updateByGuid() handles evil history entries correctly.
      db.updateByGuid(h2.guid, cv);

      // Query history table.
      Cursor cur = getAllHistory();
      int numHistory = cur.getCount();

      // Ensure only the evil history entry is marked for deletion.
      try {
        cur.moveToFirst();
        while (!cur.isAfterLast()) {
          String guid = RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
          boolean deleted = RepoUtils.getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) == 1;

          if (guid.equals(h2.guid)) {
            assertTrue(deleted);
          } else {
            assertFalse(deleted);
          }
          cur.moveToNext();
        }
      } finally {
        cur.close();
      }

      // Test 2 - Ensure purgeDelete()'s call to delete() deletes only 1 record.
      try {
        db.purgeDeleted();
      } catch (NullCursorException e) {
        e.printStackTrace();
      }

      cur = getAllHistory();
      int numHistoryAfterDeletion = cur.getCount();

      // Ensure we have only 1 deleted row.
      assertEquals(numHistoryAfterDeletion, numHistory - 1);

      // Ensure only the evil history is deleted.
      try {
        cur.moveToFirst();
        while (!cur.isAfterLast()) {
          String guid = RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
          boolean deleted = RepoUtils.getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) == 1;

          if (guid.equals(h2.guid)) {
            fail("Evil guid was not deleted!");
          } else {
            assertFalse(deleted);
          }
          cur.moveToNext();
        }
      } finally {
        cur.close();
      }
    } finally {
      closeDataAccessor(db);
      session.abort();
    }
  }

  protected Cursor getAllHistory() {
    Context context = getApplicationContext();
    Cursor cur = context.getContentResolver().query(BrowserContractHelpers.HISTORY_CONTENT_URI,
        BrowserContractHelpers.HistoryColumns, null, null, null);
    return cur;
  }

  public void testDataAccessorBulkInsert() throws NullCursorException {
    final AndroidBrowserHistoryRepositorySession session = (AndroidBrowserHistoryRepositorySession) createAndBeginSession();
    AndroidBrowserHistoryDataAccessor db = (AndroidBrowserHistoryDataAccessor) session.getDBHelper();

    ArrayList<HistoryRecord> records = new ArrayList<HistoryRecord>();
    records.add(HistoryHelpers.createHistory1());
    records.add(HistoryHelpers.createHistory2());
    records.add(HistoryHelpers.createHistory3());
    db.bulkInsert(records);

    performWait(fetchAllRunnable(session, preparedExpectFetchDelegate(records.toArray(new Record[records.size()]))));
    session.abort();
  }
}
