/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.upload.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.HashSet;

import org.junit.runner.RunWith;
import org.mozilla.gecko.background.healthreport.upload.AndroidSubmissionClient.SubmissionsFieldName;
import org.mozilla.gecko.background.healthreport.upload.test.MockAndroidSubmissionClient;
import org.mozilla.gecko.background.healthreport.upload.test.MockAndroidSubmissionClient.MockHealthReportStorage;
import org.mozilla.gecko.background.healthreport.upload.test.MockAndroidSubmissionClient.MockSubmissionsTracker;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestSubmissionsTracker {
  protected static class MockHealthReportStorage2 extends MockHealthReportStorage {
    public final int FIRST_ATTEMPT_ID = SubmissionsFieldName.FIRST_ATTEMPT.getID(this);
    public final int CONTINUATION_ATTEMPT_ID = SubmissionsFieldName.CONTINUATION_ATTEMPT.getID(this);

    public final int SUCCESS_ID = SubmissionsFieldName.SUCCESS.getID(this);
    public final int CLIENT_FAILURE_ID = SubmissionsFieldName.CLIENT_FAILURE.getID(this);
    public final int TRANSPORT_FAILURE_ID = SubmissionsFieldName.TRANSPORT_FAILURE.getID(this);
    public final int SERVER_FAILURE_ID = SubmissionsFieldName.SERVER_FAILURE.getID(this);

    private final HashSet<Integer> GUARDED_FIELDS = new HashSet<Integer>(Arrays.asList(new Integer[] {
        SUCCESS_ID,
        CLIENT_FAILURE_ID,
        TRANSPORT_FAILURE_ID,
        SERVER_FAILURE_ID
    }));

    public int incrementedFieldID = -1;
    private boolean hasIncrementedGuardedFields = false;

    @Override
    public void incrementDailyCount(int env, int day, int field) {
      incrementedFieldID = field;

      if (GUARDED_FIELDS.contains(field)) {
        if (hasIncrementedGuardedFields) {
          fail("incrementDailyCount called twice with the same guarded field.");
        }
        hasIncrementedGuardedFields = true;
      }
    }
  }

  private MockHealthReportStorage2 storage;
  private MockAndroidSubmissionClient client;
  private MockSubmissionsTracker tracker;

  @Before
  public void setUp() throws Exception {
    storage = new MockHealthReportStorage2();
    client = new MockAndroidSubmissionClient(null, null, null);
    tracker = client.new MockSubmissionsTracker(storage, 1, false);
    storage.incrementedFieldID = -1; // Reset since this is overwritten in the constructor.
  }

  @Test
  public void testIncrementFirstUploadAttemptCount() throws Exception {
    storage = new MockHealthReportStorage2();
    client = new MockAndroidSubmissionClient(null, null, null);
    tracker = client.new MockSubmissionsTracker(storage, 1, false);
    assertEquals(storage.FIRST_ATTEMPT_ID, storage.incrementedFieldID);
  }

  @Test
  public void testIncrementContinuationAttemptCount() throws Exception {
    storage = new MockHealthReportStorage2();
    client = new MockAndroidSubmissionClient(null, null, null);
    tracker = client.new MockSubmissionsTracker(storage, 1, true);
    assertEquals(storage.CONTINUATION_ATTEMPT_ID, storage.incrementedFieldID);
  }

  @Test
  public void testIncrementUploadSuccessCount() throws Exception {
    tracker.incrementUploadSuccessCount();
    assertEquals(storage.SUCCESS_ID, storage.incrementedFieldID);
  }

  @Test
  public void testIncrementClientFailureCount() throws Exception {
    tracker.incrementUploadClientFailureCount();
    assertEquals(storage.CLIENT_FAILURE_ID, storage.incrementedFieldID);
  }

  @Test
  public void testIncrementServerFailureCount() throws Exception {
    tracker.incrementUploadServerFailureCount();
    assertEquals(storage.SERVER_FAILURE_ID, storage.incrementedFieldID);
  }

  @Test
  public void testIncrementUploadTransportFailureCount() throws Exception {
    tracker.incrementUploadTransportFailureCount();
    assertEquals(storage.TRANSPORT_FAILURE_ID, storage.incrementedFieldID);
  }

  @Test
  public void testIncrementCountGuardedFields() throws Exception {
    // If the storage instance is incremented twice, the overridden HealthReportStorage methods
    // will trip a failure assertion.
    tracker.incrementUploadSuccessCount();
    tracker.incrementUploadClientFailureCount();
    tracker.incrementUploadServerFailureCount();
    tracker.incrementUploadTransportFailureCount();

    // The first field incremented should be the only (and thus last) one incremented.
    assertEquals(storage.SUCCESS_ID, storage.incrementedFieldID);
  }
}
