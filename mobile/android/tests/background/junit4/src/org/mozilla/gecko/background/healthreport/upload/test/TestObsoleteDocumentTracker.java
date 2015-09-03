/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.upload.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.AbstractMap.SimpleImmutableEntry;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map.Entry;
import java.util.Set;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.upload.ObsoleteDocumentTracker;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import android.content.SharedPreferences;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestObsoleteDocumentTracker {
  public static class MockObsoleteDocumentTracker extends ObsoleteDocumentTracker {
    public MockObsoleteDocumentTracker(SharedPreferences sharedPrefs) {
      super(sharedPrefs);
    }

    @Override
    public ExtendedJSONObject getObsoleteIds() {
      return super.getObsoleteIds();
    }

    @Override
    public void setObsoleteIds(ExtendedJSONObject ids) {
      super.setObsoleteIds(ids);
    }
};
  public MockObsoleteDocumentTracker tracker;
  public MockSharedPreferences sharedPrefs;

  @Before
  public void setUp() {
    sharedPrefs = new MockSharedPreferences();
    tracker = new MockObsoleteDocumentTracker(sharedPrefs);
  }

  @Test
  public void testDecrementObsoleteIdAttempts() {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    ids.put("id2", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    tracker.setObsoleteIds(ids);
    assertEquals(ids, tracker.getObsoleteIds());

    tracker.decrementObsoleteIdAttempts("id1");
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID - 1);
    assertEquals(ids, tracker.getObsoleteIds());

    for (int i = 0; i < HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID; i++) {
      tracker.decrementObsoleteIdAttempts("id1");
    }
    ids.remove("id1");
    assertEquals(ids, tracker.getObsoleteIds());

    tracker.removeObsoleteId("id2");
    ids.remove("id2");
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testAddObsoleteId() {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    tracker.addObsoleteId("id1");
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testDecrementObsoleteIdAttemptsSet() {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", 5L);
    ids.put("id2", 1L);
    ids.put("id3", -1L); // This should never happen, but just in case.
    tracker.setObsoleteIds(ids);
    assertEquals(ids, tracker.getObsoleteIds());

    HashSet<String> oldIds = new HashSet<String>();
    oldIds.add("id1");
    oldIds.add("id2");
    tracker.decrementObsoleteIdAttempts(oldIds);
    ids.put("id1", 4L);
    ids.remove("id2");
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testMaximumObsoleteIds() {
    for (int i = 1; i < HealthReportConstants.MAXIMUM_STORED_OBSOLETE_DOCUMENT_IDS + 10; i++) {
      tracker.addObsoleteId("id" + i);
      assertTrue(tracker.getObsoleteIds().size() <= HealthReportConstants.MAXIMUM_STORED_OBSOLETE_DOCUMENT_IDS);
    }
  }

  @Test
  public void testMigration() {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    assertEquals(ids, tracker.getObsoleteIds());

    sharedPrefs.edit()
      .putString(HealthReportConstants.PREF_LAST_UPLOAD_DOCUMENT_ID, "id")
      .commit();

    ids.put("id", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    assertEquals(ids, tracker.getObsoleteIds());

    assertTrue(sharedPrefs.contains(HealthReportConstants.PREF_OBSOLETE_DOCUMENT_IDS_TO_DELETION_ATTEMPTS_REMAINING));
  }

  @Test
  public void testGetBatchOfObsoleteIds() {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    for (int i = 0; i < 2 * HealthReportConstants.MAXIMUM_DELETIONS_PER_POST + 10; i++) {
      ids.put("id" + (100 - i), Long.valueOf(100 - i));
    }
    tracker.setObsoleteIds(ids);

    Set<String> expected = new HashSet<String>();
    for (int i = 0; i < HealthReportConstants.MAXIMUM_DELETIONS_PER_POST; i++) {
      expected.add("id" + (100 - i));
    }
    assertEquals(expected, new HashSet<String>(tracker.getBatchOfObsoleteIds()));
  }

  @Test
  public void testPairComparator() {
    // Make sure that malformed entries get sorted first.
    ArrayList<Entry<String, Object>> list = new ArrayList<Entry<String,Object>>();
    list.add(new SimpleImmutableEntry<String, Object>("a", null));
    list.add(new SimpleImmutableEntry<String, Object>("d", Long.valueOf(5)));
    list.add(new SimpleImmutableEntry<String, Object>("e", Long.valueOf(1)));
    list.add(new SimpleImmutableEntry<String, Object>("c", Long.valueOf(10)));
    list.add(new SimpleImmutableEntry<String, Object>("b", "test"));
    Collections.sort(list, new ObsoleteDocumentTracker.PairComparator());

    ArrayList<String> got = new ArrayList<String>();
    for (Entry<String, Object> pair : list) {
      got.add(pair.getKey());
    }
    List<String> exp = Arrays.asList(new String[] { "a", "b", "c", "d", "e" });
    assertEquals(exp, got);
  }

  @Test
  public void testLimitObsoleteIds() {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    for (long i = -HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID; i < HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID; i++) {
      long j = 1 + HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i;
      ids.put("id" + j, j);
    }
    tracker.setObsoleteIds(ids);
    tracker.limitObsoleteIds();

    assertEquals(ids.keySet(), tracker.getObsoleteIds().keySet());
    ExtendedJSONObject got = tracker.getObsoleteIds();
    for (String id : ids.keySet()) {
      assertTrue(got.getLong(id).longValue() <= HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    }
  }
}
