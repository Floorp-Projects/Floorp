/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.upload.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.net.UnknownHostException;
import java.util.Collection;
import java.util.HashSet;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.upload.SubmissionClient;
import org.mozilla.gecko.background.healthreport.upload.SubmissionPolicy;
import org.mozilla.gecko.background.healthreport.upload.test.TestObsoleteDocumentTracker.MockObsoleteDocumentTracker;
import org.mozilla.gecko.background.healthreport.upload.test.TestSubmissionPolicy.MockSubmissionClient.Response;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import android.content.SharedPreferences;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestSubmissionPolicy {
  public static class MockSubmissionClient implements SubmissionClient {
    public String lastId = null;
    public Collection<String> lastOldIds = null;

    public enum Response { SUCCESS, SOFT_FAILURE, HARD_FAILURE };
    public Response upload = Response.SUCCESS;
    public Response delete = Response.SUCCESS;
    public Exception exception = null;

    protected void response(long localTime, String id, Delegate delegate, Response response) {
      lastId = id;
      switch (response) {
      case SOFT_FAILURE:
        delegate.onSoftFailure(localTime, id, "Soft failure.", exception);
        break;
      case HARD_FAILURE:
        delegate.onHardFailure(localTime, id, "Hard failure.", exception);
        break;
      default:
        delegate.onSuccess(localTime, id);
      }
    }

    @Override
    public void upload(long localTime, String id, Collection<String> oldIds, Delegate delegate) {
      lastOldIds = oldIds;
      response(localTime, id, delegate, upload);
    }

    @Override
    public void delete(long localTime, String id, Delegate delegate) {
      response(localTime, id, delegate, delete);
    }
  }

  public MockSubmissionClient client;
  public SubmissionPolicy policy;
  public SharedPreferences sharedPrefs;
  public MockObsoleteDocumentTracker tracker;


  public void setMinimumTimeBetweenUploads(long time) {
    sharedPrefs.edit().putLong(HealthReportConstants.PREF_MINIMUM_TIME_BETWEEN_UPLOADS, time).commit();
  }

  public void setMinimumTimeBeforeFirstSubmission(long time) {
    sharedPrefs.edit().putLong(HealthReportConstants.PREF_MINIMUM_TIME_BEFORE_FIRST_SUBMISSION, time).commit();
  }

  public void setCurrentDayFailureCount(long count) {
    sharedPrefs.edit().putLong(HealthReportConstants.PREF_CURRENT_DAY_FAILURE_COUNT, count).commit();
  }

  @Before
  public void setUp() throws Exception {
    sharedPrefs = new MockSharedPreferences();
    client = new MockSubmissionClient();
    tracker = new MockObsoleteDocumentTracker(sharedPrefs);
    policy = new SubmissionPolicy(sharedPrefs, client, tracker, true);
    setMinimumTimeBeforeFirstSubmission(0);
  }

  @Test
  public void testNoMinimumTimeBeforeFirstSubmission() {
    assertTrue(policy.tick(0));
  }

  @Test
  public void testMinimumTimeBeforeFirstSubmission() {
    setMinimumTimeBeforeFirstSubmission(10);
    assertFalse(policy.tick(0));
    assertEquals(policy.getMinimumTimeBeforeFirstSubmission(), policy.getNextSubmission());
    assertFalse(policy.tick(policy.getMinimumTimeBeforeFirstSubmission() - 1));
    assertTrue(policy.tick(policy.getMinimumTimeBeforeFirstSubmission()));
  }

  @Test
  public void testNextUpload() {
    assertTrue(policy.tick(0));
    assertEquals(policy.getMinimumTimeBetweenUploads(), policy.getNextSubmission());
    assertFalse(policy.tick(policy.getMinimumTimeBetweenUploads() - 1));
    assertTrue(policy.tick(policy.getMinimumTimeBetweenUploads()));
  }

  @Test
  public void testLastUploadRequested() {
    assertTrue(policy.tick(0));
    assertEquals(0, policy.getLastUploadRequested());
    assertFalse(policy.tick(1));
    assertEquals(0, policy.getLastUploadRequested());
    assertTrue(policy.tick(2*policy.getMinimumTimeBetweenUploads()));
    assertEquals(2*policy.getMinimumTimeBetweenUploads(), policy.getLastUploadRequested());
  }

  @Test
  public void testUploadSuccess() throws Exception {
    assertTrue(policy.tick(0));
    setCurrentDayFailureCount(1);
    client.upload = Response.SUCCESS;
    assertTrue(policy.tick(2*policy.getMinimumTimeBetweenUploads()));
    assertEquals(2*policy.getMinimumTimeBetweenUploads(), policy.getLastUploadRequested());
    assertEquals(2*policy.getMinimumTimeBetweenUploads(), policy.getLastUploadSucceeded());
    assertTrue(policy.getNextSubmission() > policy.getLastUploadSucceeded());
    assertEquals(0, policy.getCurrentDayFailureCount());
    assertNotNull(client.lastId);
    assertTrue(tracker.getObsoleteIds().containsKey(client.lastId));
  }

  @Test
  public void testUploadSoftFailure() throws Exception {
    final long initialUploadTime = 0;
    assertTrue(policy.tick(initialUploadTime));
    client.upload = Response.SOFT_FAILURE;

    final long secondUploadTime = initialUploadTime + policy.getMinimumTimeBetweenUploads();
    assertTrue(policy.tick(secondUploadTime));
    assertEquals(secondUploadTime, policy.getLastUploadRequested());
    assertEquals(secondUploadTime, policy.getLastUploadFailed());
    assertEquals(1, policy.getCurrentDayFailureCount());
    assertEquals(policy.getLastUploadFailed() + policy.getMinimumTimeAfterFailure(), policy.getNextSubmission());
    assertNotNull(client.lastId);
    assertTrue(tracker.getObsoleteIds().containsKey(client.lastId));
    client.lastId = null;

    final long thirdUploadTime = secondUploadTime + policy.getMinimumTimeAfterFailure();
    assertTrue(policy.tick(thirdUploadTime));
    assertEquals(thirdUploadTime, policy.getLastUploadRequested());
    assertEquals(thirdUploadTime, policy.getLastUploadFailed());
    assertEquals(2, policy.getCurrentDayFailureCount());
    assertEquals(policy.getLastUploadFailed() + policy.getMinimumTimeAfterFailure(), policy.getNextSubmission());
    assertNotNull(client.lastId);
    assertTrue(tracker.getObsoleteIds().containsKey(client.lastId));
    client.lastId = null;

    final long fourthUploadTime = thirdUploadTime + policy.getMinimumTimeAfterFailure();
    assertTrue(policy.tick(fourthUploadTime));
    assertEquals(fourthUploadTime, policy.getLastUploadRequested());
    assertEquals(fourthUploadTime, policy.getLastUploadFailed());
    assertEquals(0, policy.getCurrentDayFailureCount());
    assertEquals(policy.getLastUploadFailed() + policy.getMinimumTimeBetweenUploads(), policy.getNextSubmission());
    assertNotNull(client.lastId);
    assertTrue(tracker.getObsoleteIds().containsKey(client.lastId));
    client.lastId = null;
  }

  @Test
  public void testUploadHardFailure() throws Exception {
    assertTrue(policy.tick(0));
    client.upload = Response.HARD_FAILURE;

    assertTrue(policy.tick(2*policy.getMinimumTimeBetweenUploads()));
    assertEquals(2*policy.getMinimumTimeBetweenUploads(), policy.getLastUploadRequested());
    assertEquals(2*policy.getMinimumTimeBetweenUploads(), policy.getLastUploadFailed());
    assertEquals(0, policy.getCurrentDayFailureCount());
    assertEquals(policy.getLastUploadFailed() + policy.getMinimumTimeBetweenUploads(), policy.getNextSubmission());
    assertNotNull(client.lastId);
    assertTrue(tracker.getObsoleteIds().containsKey(client.lastId));
    client.lastId = null;
  }

  @Test
  public void testDisabledNoObsoleteDocuments() throws Exception {
    policy = new SubmissionPolicy(sharedPrefs, client, tracker, false);
    assertFalse(policy.tick(0));
  }

  @Test
  public void testDisabledObsoleteDocumentsSuccess() throws Exception {
    policy = new SubmissionPolicy(sharedPrefs, client, tracker, false);
    setMinimumTimeBetweenUploads(policy.getMinimumTimeBetweenUploads() - 1);
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", 5L);
    ids.put("id2", 5L);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3));
    assertEquals(1, tracker.getObsoleteIds().size());

    // Forensic timestamps.
    assertEquals(3 + policy.getMinimumTimeBetweenDeletes(), policy.getNextSubmission());
    assertEquals(3, policy.getLastDeleteRequested());
    assertEquals(-1, policy.getLastDeleteFailed());
    assertEquals(3, policy.getLastDeleteSucceeded());

    assertTrue(policy.tick(2*policy.getMinimumTimeBetweenDeletes()));
    assertEquals(0, tracker.getObsoleteIds().size());

    assertFalse(policy.tick(4*policy.getMinimumTimeBetweenDeletes()));
  }

  @Test
  public void testDisabledObsoleteDocumentsSoftFailure() throws Exception {
    client.delete = Response.SOFT_FAILURE;
    policy = new SubmissionPolicy(sharedPrefs, client, tracker, false);
    setMinimumTimeBetweenUploads(policy.getMinimumTimeBetweenUploads() - 2);
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", 5L);
    ids.put("id2", 2L);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3));
    ids.put("id1", 4L); // policy's choice is deterministic.
    assertEquals(ids, tracker.getObsoleteIds());

    // Forensic timestamps.
    assertEquals(3 + policy.getMinimumTimeBetweenDeletes(), policy.getNextSubmission());
    assertEquals(3, policy.getLastDeleteRequested());
    assertEquals(3, policy.getLastDeleteFailed());
    assertEquals(-1, policy.getLastDeleteSucceeded());

    assertTrue(policy.tick(2*policy.getMinimumTimeBetweenDeletes())); // 3, 3
    ids.put("id1", 3L);
    assertEquals(ids, tracker.getObsoleteIds());

    assertTrue(policy.tick(4*policy.getMinimumTimeBetweenDeletes()));
    assertTrue(policy.tick(6*policy.getMinimumTimeBetweenDeletes()));
    assertTrue(policy.tick(8*policy.getMinimumTimeBetweenDeletes()));
    assertTrue(policy.tick(10*policy.getMinimumTimeBetweenDeletes()));
    ids.put("id1", 1L);
    ids.remove("id2");
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testDisabledObsoleteDocumentsHardFailure() throws Exception {
    client.delete = Response.HARD_FAILURE;
    policy = new SubmissionPolicy(sharedPrefs, client, tracker, false);
    setMinimumTimeBetweenUploads(policy.getMinimumTimeBetweenUploads() - 3);
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", 5L);
    ids.put("id2", 2L);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3));
    ids.remove("id1"); // policy's choice is deterministic.
    assertEquals(ids, tracker.getObsoleteIds());

    // Forensic timestamps.
    assertEquals(3 + policy.getMinimumTimeBetweenDeletes(), policy.getNextSubmission());
    assertEquals(3, policy.getLastDeleteRequested());
    assertEquals(3, policy.getLastDeleteFailed());
    assertEquals(-1, policy.getLastDeleteSucceeded());

    assertTrue(policy.tick(2*policy.getMinimumTimeBetweenDeletes())); // 3.
    ids.remove("id2");
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testUploadSuccessMultipleObsoletes() throws Exception {
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    ids.put("id2", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3));
    assertEquals(ids.keySet(), new HashSet<String>(client.lastOldIds));
    assertNotNull(client.lastId);
    ids.remove("id1");
    ids.remove("id2");
    ids.put(client.lastId, HealthReportConstants.DELETION_ATTEMPTS_PER_KNOWN_TO_BE_ON_SERVER_DOCUMENT_ID);
    assertEquals(ids, tracker.getObsoleteIds());

    ExtendedJSONObject ids1 = new ExtendedJSONObject(); // First half.
    ExtendedJSONObject ids2 = new ExtendedJSONObject(); // Second half.
    ExtendedJSONObject ids3 = new ExtendedJSONObject(); // Both halves.
    for (int i = 0; i < HealthReportConstants.MAXIMUM_DELETIONS_PER_POST; i++) {
      String id1 = "x" + i;
      String id2 = "y" + i;
      ids1.put(id1, 3*HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
      ids2.put(id2, HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
      ids3.put(id1, 3*HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
      ids3.put(id2, HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
    }
    tracker.setObsoleteIds(ids3);

    assertTrue(policy.tick(3 + policy.getMinimumTimeBetweenUploads()));
    assertNotNull(client.lastId);
    assertEquals(ids1.keySet(), new HashSet<String>(client.lastOldIds));
    ids2.put(client.lastId, HealthReportConstants.DELETION_ATTEMPTS_PER_KNOWN_TO_BE_ON_SERVER_DOCUMENT_ID);
    assertEquals(ids2, tracker.getObsoleteIds());
  }

  @Test
  public void testUploadFailureMultipleObsoletes() throws Exception {
    client.upload = Response.HARD_FAILURE;
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    ids.put("id2", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3));
    assertEquals(ids.keySet(), new HashSet<String>(client.lastOldIds));
    assertNotNull(client.lastId);
    ids.put(client.lastId, HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID - 1);
    ids.put("id2", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID - 1);
    assertEquals(ids, tracker.getObsoleteIds());

    ExtendedJSONObject ids1 = new ExtendedJSONObject(); // First half.
    ExtendedJSONObject ids2 = new ExtendedJSONObject(); // Second half.
    ExtendedJSONObject ids3 = new ExtendedJSONObject(); // Both halves.
    for (int i = 0; i < HealthReportConstants.MAXIMUM_DELETIONS_PER_POST; i++) {
      String id1 = "x" + i;
      String id2 = "y" + i;
      ids1.put(id1, 3*HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
      ids2.put(id2, HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
      ids3.put(id1, 3*HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
      ids3.put(id2, HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID + i);
    }
    tracker.setObsoleteIds(ids3);

    assertTrue(policy.tick(3 + policy.getMinimumTimeBetweenUploads()));
    assertEquals(ids1.keySet(), new HashSet<String>(client.lastOldIds));
    assertNotNull(client.lastId);
    ids3.put(client.lastId, HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    for (String id : ids1.keySet()) {
      ids3.put(id, ids1.getLong(id) - 1);
    }
    assertEquals(ids3, tracker.getObsoleteIds());
  }

  @Test
  public void testUploadLocalFailure() throws Exception {
    client.upload = Response.HARD_FAILURE;
    client.exception = new UnknownHostException();

    // We shouldn't add an id for a local exception.
    assertFalse(tracker.hasObsoleteIds());
    assertTrue(policy.tick(3));
    assertFalse(tracker.hasObsoleteIds());

    // And we shouldn't decrement the obsolete-delete attempts map.
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    ids.put("id2", HealthReportConstants.DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3 + policy.getMinimumTimeBetweenUploads()));
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testDisabledLocalFailure() throws Exception {
    client.delete = Response.SOFT_FAILURE;
    client.exception = new UnknownHostException();

    policy = new SubmissionPolicy(sharedPrefs, client, tracker, false);
    setMinimumTimeBetweenUploads(policy.getMinimumTimeBetweenUploads() - 1);
    ExtendedJSONObject ids = new ExtendedJSONObject();
    ids.put("id1", 5L);
    ids.put("id2", 3L);
    tracker.setObsoleteIds(ids);

    assertTrue(policy.tick(3));
    // The upload fails locally, so we shouldn't decrement attempts remaining.
    assertEquals(ids, tracker.getObsoleteIds());
  }

  @Test
  public void testUploadFailureCountResetAfterOneDay() throws Exception {
    client.upload = Response.SOFT_FAILURE;
    assertEquals(0, policy.getCurrentDayFailureCount());
    assertEquals(-1, policy.getCurrentDayResetTime());

    final long initialUploadTime = 0;
    assertTrue(policy.tick(initialUploadTime));
    final long initialResetTime = initialUploadTime + policy.getMinimumTimeBetweenUploads();
    assertEquals(initialResetTime, policy.getCurrentDayResetTime());
    assertEquals(1, policy.getCurrentDayFailureCount());

    final long secondUploadTime = initialUploadTime + policy.getMinimumTimeAfterFailure();
    assertTrue(policy.tick(secondUploadTime));
    assertEquals(initialResetTime, policy.getCurrentDayResetTime());
    assertEquals(2, policy.getCurrentDayFailureCount());

    assertTrue(policy.tick(initialResetTime));
    final long secondResetTime = initialResetTime + policy.getMinimumTimeBetweenUploads();
    assertEquals(secondResetTime, policy.getCurrentDayResetTime());
    assertEquals(1, policy.getCurrentDayFailureCount());
  }

  @Test
  public void testUploadFailureCountResetAfterUploadSuccess() throws Exception {
    client.upload = Response.SOFT_FAILURE;
    final long uploadTime = 0;
    assertTrue(policy.tick(uploadTime));
    assertEquals(1, policy.getCurrentDayFailureCount());

    client.upload = Response.SUCCESS;
    assertTrue(policy.tick(uploadTime + policy.getMinimumTimeAfterFailure()));
    assertEquals(-1, policy.getCurrentDayResetTime());
    assertEquals(0, policy.getCurrentDayFailureCount());
  }

  @Test
  public void testUploadFailureCountResetAfterUploadHardFailure() throws Exception {
    client.upload = Response.SOFT_FAILURE;
    final long uploadTime = 0;
    assertTrue(policy.tick(uploadTime));
    assertEquals(1, policy.getCurrentDayFailureCount());

    client.upload = Response.HARD_FAILURE;
    assertTrue(policy.tick(uploadTime + policy.getMinimumTimeAfterFailure()));
    assertEquals(-1, policy.getCurrentDayResetTime());
    assertEquals(0, policy.getCurrentDayFailureCount());
  }
}
