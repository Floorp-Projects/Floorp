/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.upload;

import java.util.Collection;

import org.mozilla.gecko.background.bagheera.BagheeraRequestDelegate;
import org.mozilla.gecko.background.healthreport.Environment;
import org.mozilla.gecko.background.healthreport.HealthReportStorage;
import org.mozilla.gecko.background.healthreport.HealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.MockHealthReportDatabaseStorage.PrepopulatedMockHealthReportDatabaseStorage;
import org.mozilla.gecko.background.healthreport.MockProfileInformationCache;
import org.mozilla.gecko.background.healthreport.ProfileInformationCache;
import org.mozilla.gecko.background.healthreport.upload.AndroidSubmissionClient;
import org.mozilla.gecko.background.healthreport.upload.AndroidSubmissionClient.SubmissionsFieldName;
import org.mozilla.gecko.background.helpers.FakeProfileTestCase;
import org.mozilla.gecko.background.testhelpers.StubDelegate;

import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SharedPreferences;
import org.json.JSONException;
import org.json.JSONObject;

public class TestAndroidSubmissionClient extends FakeProfileTestCase {
  public static class MockAndroidSubmissionClient extends AndroidSubmissionClient {
    protected final PrepopulatedMockHealthReportDatabaseStorage storage;

    public SubmissionState submissionState = SubmissionState.SUCCESS;
    public DocumentStatus documentStatus = DocumentStatus.VALID;
    public boolean hasUploadBeenRequested = true;

    public MockAndroidSubmissionClient(final Context context, final SharedPreferences sharedPrefs,
        final PrepopulatedMockHealthReportDatabaseStorage storage) {
      super(context, sharedPrefs, "profilePath");
      this.storage = storage;
    }

    @Override
    public HealthReportDatabaseStorage getStorage(final ContentProviderClient client) {
      return storage;
    }

    @Override
    public boolean hasUploadBeenRequested() {
      return hasUploadBeenRequested;
    }

    @Override
    protected void uploadPayload(String id, String payload, Collection<String> oldIds,
        BagheeraRequestDelegate delegate) {
      switch (submissionState) {
      case SUCCESS:
        delegate.handleSuccess(0, null, id, null);
        break;

      case FAILURE:
        delegate.handleFailure(0, null, null);
        break;

      case ERROR:
        delegate.handleError(null);
        break;

      default:
        throw new IllegalStateException("Unknown submissionState, " + submissionState);
      }
    }

    @Override
    public SubmissionsTracker getSubmissionsTracker(final HealthReportStorage storage,
        final long localTime, final boolean hasUploadBeenRequested) {
      return new MockSubmissionsTracker(storage, localTime, hasUploadBeenRequested);
    }

    public class MockSubmissionsTracker extends SubmissionsTracker {
      public MockSubmissionsTracker (final HealthReportStorage storage, final long localTime,
          final boolean hasUploadBeenRequested) {
        super(storage, localTime, hasUploadBeenRequested);
      }

      @Override
      public ProfileInformationCache getProfileInformationCache() {
        final MockProfileInformationCache cache = new MockProfileInformationCache(profilePath);
        cache.setInitialized(true); // Will throw errors otherwise.
        return cache;
      }

      @Override
      public TrackingGenerator getGenerator() {
        return new MockTrackingGenerator();
      }

      public class MockTrackingGenerator extends TrackingGenerator {
        @Override
        public JSONObject generateDocument(final long localTime, final long last,
            final String profilePath) throws JSONException {
          switch (documentStatus) {
          case VALID:
            return new JSONObject(); // Beyond == null, we don't check for valid FHR documents.

          case NULL:
            // The overridden method should return null since we return a null has for the current
            // Environment.
            return super.generateDocument(localTime, last, profilePath);

          case EXCEPTION:
            throw new IllegalStateException("Intended Exception");

          default:
            throw new IllegalStateException("Unintended Exception");
          }
        }

        // Used in super.generateDocument, where a null document is returned if getHash returns null
        @Override
        public Environment getCurrentEnvironment() {
          return new Environment() {
            @Override
            public int register() {
              return 0;
            }

            @Override
            public String getHash() {
              return null;
            }
          };
        }
      }
    }
  }

  public final SubmissionsFieldName[] SUBMISSIONS_STATUS_FIELD_NAMES = new SubmissionsFieldName[] {
    SubmissionsFieldName.SUCCESS,
    SubmissionsFieldName.CLIENT_FAILURE,
    SubmissionsFieldName.TRANSPORT_FAILURE,
    SubmissionsFieldName.SERVER_FAILURE
  };

  public static enum SubmissionState { SUCCESS, FAILURE, ERROR }
  public static enum DocumentStatus { VALID, NULL, EXCEPTION };

  public StubDelegate stubDelegate;
  public PrepopulatedMockHealthReportDatabaseStorage storage;
  public MockAndroidSubmissionClient client;

  public void setUp() throws Exception {
    super.setUp();
    stubDelegate = new StubDelegate();
    storage = new PrepopulatedMockHealthReportDatabaseStorage(context, fakeProfileDirectory);
    client = new MockAndroidSubmissionClient(context, getSharedPreferences(), storage);
  }

  public int getSubmissionsCount(final SubmissionsFieldName fieldName) {
    final int id = fieldName.getID(storage);
    return storage.getIntFromQuery("SELECT COUNT(*) FROM events WHERE field = " + id, null);
  }

  public void testUploadSubmissionsFirstAttemptCount() throws Exception {
    client.hasUploadBeenRequested = false;
    client.upload(storage.now, null, null, stubDelegate);
    assertEquals(1, getSubmissionsCount(SubmissionsFieldName.FIRST_ATTEMPT));
    assertEquals(0, getSubmissionsCount(SubmissionsFieldName.CONTINUATION_ATTEMPT));
  }

  public void testUploadSubmissionsContinuationAttemptCount() throws Exception {
    client.upload(storage.now, null, null, stubDelegate);
    assertEquals(0, getSubmissionsCount(SubmissionsFieldName.FIRST_ATTEMPT));
    assertEquals(1, getSubmissionsCount(SubmissionsFieldName.CONTINUATION_ATTEMPT));
  }

  /**
   * Asserts that the given field name has a count of 1 and all other status (success and failures)
   * have a count of 0.
   */
  public void assertStatusCount(final SubmissionsFieldName fieldName) {
    for (SubmissionsFieldName name : SUBMISSIONS_STATUS_FIELD_NAMES) {
      if (name == fieldName) {
        assertEquals(1, getSubmissionsCount(name));
      } else {
        assertEquals(0, getSubmissionsCount(name));
      }
    }
  }

  public void testUploadSubmissionsSuccessCount() throws Exception {
    client.upload(storage.now, null, null, stubDelegate);
    assertStatusCount(SubmissionsFieldName.SUCCESS);
  }

  public void testUploadNullDocumentSubmissionsFailureCount() throws Exception {
    client.documentStatus = DocumentStatus.NULL;
    client.upload(storage.now, null, null, stubDelegate);
    assertStatusCount(SubmissionsFieldName.CLIENT_FAILURE);
  }

  public void testUploadDocumentGenerationExceptionSubmissionsFailureCount() throws Exception {
    client.documentStatus = DocumentStatus.EXCEPTION;
    client.upload(storage.now, null, null, stubDelegate);
    assertStatusCount(SubmissionsFieldName.CLIENT_FAILURE);
  }
}
