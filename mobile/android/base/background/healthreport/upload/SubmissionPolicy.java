/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.upload;

import java.net.MalformedURLException;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.Collection;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.HealthReportUtils;
import org.mozilla.gecko.background.healthreport.upload.SubmissionClient.Delegate;

import android.content.SharedPreferences;

/**
 * Manages scheduling of Firefox Health Report data submission.
 *
 * The rules of data submission are as follows:
 *
 * 1. Do not submit data more than once every 24 hours.
 *
 * 2. Try to submit as close to 24 hours apart as possible.
 *
 * 3. Do not submit too soon after application startup so as to not negatively
 * impact performance at startup.
 *
 * 4. Before first ever data submission, the user should be notified about data
 * collection practices.
 *
 * 5. User should have opportunity to react to this notification before data
 * submission.
 *
 * 6. Display of notification without any explicit user action constitutes
 * implicit consent after a certain duration of time.
 *
 * 7. If data submission fails, try at most 2 additional times before giving up
 * on that day's submission.
 *
 * On Android, items 4, 5, and 6 are addressed by displaying an Android
 * notification on first run.
 */
public class SubmissionPolicy {
  public static final String LOG_TAG = SubmissionPolicy.class.getSimpleName();

  protected final SharedPreferences sharedPreferences;
  protected final SubmissionClient client;
  protected final boolean uploadEnabled;
  protected final ObsoleteDocumentTracker tracker;

  public SubmissionPolicy(final SharedPreferences sharedPreferences,
      final SubmissionClient client,
      final ObsoleteDocumentTracker tracker,
      boolean uploadEnabled) {
    if (sharedPreferences == null) {
      throw new IllegalArgumentException("sharedPreferences must not be null");
    }
    this.sharedPreferences = sharedPreferences;
    this.client = client;
    this.tracker = tracker;
    this.uploadEnabled = uploadEnabled;
  }

  /**
   * Check what action must happen, advance counters and timestamps, and
   * possibly spawn a request to the server.
   *
   * @param localTime now.
   * @return true if a request was spawned; false otherwise.
   */
  public boolean tick(final long localTime) {
    final long nextUpload = getNextSubmission();

    // If the system clock were ever set to a time in the distant future,
    // it's possible our next schedule date is far out as well. We know
    // we shouldn't schedule for more than a day out, so we reset the next
    // scheduled date appropriately. 3 days was chosen to match desktop's
    // arbitrary choice.
    if (nextUpload >= localTime + 3 * getMinimumTimeBetweenUploads()) {
      Logger.warn(LOG_TAG, "Next upload scheduled far in the future; system clock reset? " + nextUpload + " > " + localTime);
      // Things are strange, we want to start again but we don't want to stampede.
      editor()
        .setNextSubmission(localTime + getMinimumTimeBetweenUploads())
        .commit();
      return false;
    }

    // Don't upload unless an interval has elapsed.
    if (localTime < nextUpload) {
      Logger.debug(LOG_TAG, "We uploaded less than an interval ago; skipping. " + nextUpload + " > " + localTime);
      return false;
    }

    if (!uploadEnabled) {
      // We only delete (rather than mark as obsolete during upload) when
      // uploading is disabled. We try to delete aggressively, since the volume
      // of deletes should be very low. But we don't want to send too many
      // delete requests at the same time, so we process these one at a time. In
      // the future (Bug 872756), we will be able to delete multiple documents
      // with one request.
      final String obsoleteId = tracker.getNextObsoleteId();
      if (obsoleteId == null) {
        return false;
      }

      Editor editor = editor();
      editor.setLastDeleteRequested(localTime); // Write committed by delegate.
      client.delete(localTime, obsoleteId, new DeleteDelegate(editor));
      return true;
    }

    long firstRun = getFirstRunLocalTime();
    if (firstRun < 0) {
      firstRun = localTime;
      // Make sure we start clean and as soon as possible.
      editor()
        .setFirstRunLocalTime(firstRun)
        .setNextSubmission(localTime + getMinimumTimeBeforeFirstSubmission())
        .setCurrentDayFailureCount(0)
        .commit();
    }

    // This case will occur if the nextSubmission time is not set (== -1) but firstRun is.
    if (localTime < firstRun + getMinimumTimeBeforeFirstSubmission()) {
      Logger.info(LOG_TAG, "Need to wait " + getMinimumTimeBeforeFirstSubmission() + " before first upload.");
      return false;
    }

    // The first upload attempt for a given document submission begins a 24-hour period in which
    // the upload will retry upon a soft failure. At the end of this period, the submission
    // failure count is reset, ensuring each day's first submission attempt has a zeroed failure
    // count. A period may also end on upload success or hard failure.
    if (localTime >= getCurrentDayResetTime()) {
      editor()
        .setCurrentDayResetTime(localTime + getMinimumTimeBetweenUploads())
        .setCurrentDayFailureCount(0)
        .commit();
    }

    String id = HealthReportUtils.generateDocumentId();
    Collection<String> oldIds = tracker.getBatchOfObsoleteIds();
    tracker.addObsoleteId(id);

    Editor editor = editor();
    editor.setLastUploadRequested(localTime); // Write committed by delegate.
    client.upload(localTime, id, oldIds, new UploadDelegate(editor, oldIds));
    return true;
  }

  /**
   * Return true if the upload that produced <code>e</code> definitely did not
   * produce a new record on the remote server.
   *
   * @param e
   *          <code>Exception</code> that upload produced.
   * @return true if the server could not have a new record.
   */
  protected boolean isLocalException(Exception e) {
    return (e instanceof MalformedURLException) ||
           (e instanceof SocketException) ||
           (e instanceof UnknownHostException);
  }

  protected class UploadDelegate implements Delegate {
    protected final Editor editor;
    protected final Collection<String> oldIds;

    public UploadDelegate(Editor editor, Collection<String> oldIds) {
      this.editor = editor;
      this.oldIds = oldIds;
    }

    @Override
    public void onSuccess(long localTime, String id) {
      long next = localTime + getMinimumTimeBetweenUploads();
      tracker.markIdAsUploaded(id);
      tracker.purgeObsoleteIds(oldIds);
      editor
        .setNextSubmission(next)
        .setLastUploadSucceeded(localTime)
        .setCurrentDayFailureCount(0)
        .clearCurrentDayResetTime() // Set again on the next submission's first upload attempt.
        .commit();
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.pii(LOG_TAG, "Successful upload with id " + id + " obsoleting "
            + oldIds.size() + " old records reported at " + localTime + "; next upload at " + next + ".");
      } else {
        Logger.info(LOG_TAG, "Successful upload obsoleting " + oldIds.size()
            + " old records reported at " + localTime + "; next upload at " + next + ".");
      }
    }

    @Override
    public void onHardFailure(long localTime, String id, String reason, Exception e) {
      long next = localTime + getMinimumTimeBetweenUploads();
      if (isLocalException(e)) {
        Logger.info(LOG_TAG, "Hard failure caused by local exception; not tracking id and not decrementing attempts.");
        tracker.removeObsoleteId(id);
      } else {
        tracker.decrementObsoleteIdAttempts(oldIds);
      }
      editor
        .setNextSubmission(next)
        .setLastUploadFailed(localTime)
        .setCurrentDayFailureCount(0)
        .clearCurrentDayResetTime() // Set again on the next submission's first upload attempt.
        .commit();
      Logger.warn(LOG_TAG, "Hard failure reported at " + localTime + ": " + reason + " Next upload at " + next + ".", e);
    }

    @Override
    public void onSoftFailure(long localTime, String id, String reason, Exception e) {
      int failuresToday = getCurrentDayFailureCount();
      Logger.warn(LOG_TAG, "Soft failure reported at " + localTime + ": " + reason + " Previously failed " + failuresToday + " time(s) today.");

      if (failuresToday >= getMaximumFailuresPerDay()) {
        onHardFailure(localTime, id, "Reached the limit of daily upload attempts: " + failuresToday, e);
        return;
      }

      long next = localTime + getMinimumTimeAfterFailure();
      if (isLocalException(e)) {
        Logger.info(LOG_TAG, "Soft failure caused by local exception; not tracking id and not decrementing attempts.");
        tracker.removeObsoleteId(id);
      } else {
        tracker.decrementObsoleteIdAttempts(oldIds);
      }
      editor
        .setNextSubmission(next)
        .setLastUploadFailed(localTime)
        .setCurrentDayFailureCount(failuresToday + 1)
        .commit();
      Logger.info(LOG_TAG, "Retrying upload at " + next + ".");
    }
  }

  protected class DeleteDelegate implements Delegate {
    protected final Editor editor;

    public DeleteDelegate(Editor editor) {
      this.editor = editor;
    }

    @Override
    public void onSoftFailure(final long localTime, String id, String reason, Exception e) {
      long next = localTime + getMinimumTimeBetweenDeletes();
      if (isLocalException(e)) {
        Logger.info(LOG_TAG, "Soft failure caused by local exception; not decrementing attempts.");
      } else {
        tracker.decrementObsoleteIdAttempts(id);
      }
      editor
        .setNextSubmission(next)
        .setLastDeleteFailed(localTime)
        .commit();

      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.info(LOG_TAG, "Got soft failure at " + localTime + " deleting obsolete document with id " + id + ": " + reason + " Trying again later.");
      } else {
        Logger.info(LOG_TAG, "Got soft failure at " + localTime + " deleting obsolete document: " + reason + " Trying again later.");
      }
    }

    @Override
    public void onHardFailure(final long localTime, String id, String reason, Exception e) {
      // We're never going to be able to delete this id, so don't keep trying.
      long next = localTime + getMinimumTimeBetweenDeletes();
      tracker.removeObsoleteId(id);
      editor
        .setNextSubmission(next)
        .setLastDeleteFailed(localTime)
        .commit();

      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.warn(LOG_TAG, "Got hard failure at " + localTime + " deleting obsolete document with id " + id + ": " + reason + " Abandoning delete request.", e);
      } else {
        Logger.warn(LOG_TAG, "Got hard failure at " + localTime + " deleting obsolete document: " + reason + " Abandoning delete request.", e);
      }
    }

    @Override
    public void onSuccess(final long localTime, String id) {
      long next = localTime + getMinimumTimeBetweenDeletes();
      tracker.removeObsoleteId(id);
      editor
        .setNextSubmission(next)
        .setLastDeleteSucceeded(localTime)
        .commit();

      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.pii(LOG_TAG, "Deleted an obsolete document with id " + id + " at " + localTime + ".");
      } else {
        Logger.info(LOG_TAG, "Deleted an obsolete document at " + localTime + ".");
      }
    }
  }

  public SharedPreferences getSharedPreferences() {
    return this.sharedPreferences;
  }

  public long getMinimumTimeBetweenUploads() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_MINIMUM_TIME_BETWEEN_UPLOADS, HealthReportConstants.DEFAULT_MINIMUM_TIME_BETWEEN_UPLOADS);
  }

  public long getMinimumTimeBeforeFirstSubmission() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_MINIMUM_TIME_BEFORE_FIRST_SUBMISSION, HealthReportConstants.DEFAULT_MINIMUM_TIME_BEFORE_FIRST_SUBMISSION);
  }

  public long getMinimumTimeAfterFailure() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_MINIMUM_TIME_AFTER_FAILURE, HealthReportConstants.DEFAULT_MINIMUM_TIME_AFTER_FAILURE);
  }

  public long getMaximumFailuresPerDay() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_MAXIMUM_FAILURES_PER_DAY, HealthReportConstants.DEFAULT_MAXIMUM_FAILURES_PER_DAY);
  }

  // Authoritative.
  public long getFirstRunLocalTime() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_FIRST_RUN, -1);
  }

  // Authoritative.
  public long getNextSubmission() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_NEXT_SUBMISSION, -1);
  }

  // Authoritative.
  public int getCurrentDayFailureCount() {
    return getSharedPreferences().getInt(HealthReportConstants.PREF_CURRENT_DAY_FAILURE_COUNT, 0);
  }

  // Authoritative.
  public long getCurrentDayResetTime() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_CURRENT_DAY_RESET_TIME, -1);
  }

  /**
   * To avoid writing to disk multiple times, we encapsulate writes in a
   * helper class. Be sure to call <code>commit</code> to flush to disk!
   */
  protected Editor editor() {
    return new Editor(getSharedPreferences().edit());
  }

  protected static class Editor {
    protected final SharedPreferences.Editor editor;

    public Editor(SharedPreferences.Editor editor) {
      this.editor = editor;
    }

    public void commit() {
      editor.commit();
    }

    // Authoritative.
    public Editor setFirstRunLocalTime(long localTime) {
      editor.putLong(HealthReportConstants.PREF_FIRST_RUN, localTime);
      return this;
    }

    // Authoritative.
    public Editor setNextSubmission(long localTime) {
      editor.putLong(HealthReportConstants.PREF_NEXT_SUBMISSION, localTime);
      return this;
    }

    // Authoritative.
    public Editor setCurrentDayFailureCount(int failureCount) {
      editor.putInt(HealthReportConstants.PREF_CURRENT_DAY_FAILURE_COUNT, failureCount);
      return this;
    }

    // Authoritative.
    public Editor setCurrentDayResetTime(long resetTime) {
      editor.putLong(HealthReportConstants.PREF_CURRENT_DAY_RESET_TIME, resetTime);
      return this;
    }

    // Authoritative.
    public Editor clearCurrentDayResetTime() {
      editor.putLong(HealthReportConstants.PREF_CURRENT_DAY_RESET_TIME, -1);
      return this;
    }

    // Forensics only.
    public Editor setLastUploadRequested(long localTime) {
      editor.putLong(HealthReportConstants.PREF_LAST_UPLOAD_REQUESTED, localTime);
      return this;
    }

    // Forensics only.
    public Editor setLastUploadSucceeded(long localTime) {
      editor.putLong(HealthReportConstants.PREF_LAST_UPLOAD_SUCCEEDED, localTime);
      return this;
    }

    // Forensics only.
    public Editor setLastUploadFailed(long localTime) {
      editor.putLong(HealthReportConstants.PREF_LAST_UPLOAD_FAILED, localTime);
      return this;
    }

    // Forensics only.
    public Editor setLastDeleteRequested(long localTime) {
      editor.putLong(HealthReportConstants.PREF_LAST_DELETE_REQUESTED, localTime);
      return this;
    }

    // Forensics only.
    public Editor setLastDeleteSucceeded(long localTime) {
      editor.putLong(HealthReportConstants.PREF_LAST_DELETE_SUCCEEDED, localTime);
      return this;
    }

    // Forensics only.
    public Editor setLastDeleteFailed(long localTime) {
      editor.putLong(HealthReportConstants.PREF_LAST_DELETE_FAILED, localTime);
      return this;
    }
  }

  // Forensics only.
  public long getLastUploadRequested() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_UPLOAD_REQUESTED, -1);
  }

  // Forensics only.
  public long getLastUploadSucceeded() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_UPLOAD_SUCCEEDED, -1);
  }

  // Forensics only.
  public long getLastUploadFailed() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_UPLOAD_FAILED, -1);
  }

  // Forensics only.
  public long getLastDeleteRequested() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_DELETE_REQUESTED, -1);
  }

  // Forensics only.
  public long getLastDeleteSucceeded() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_DELETE_SUCCEEDED, -1);
  }

  // Forensics only.
  public long getLastDeleteFailed() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_LAST_DELETE_FAILED, -1);
  }

  public long getMinimumTimeBetweenDeletes() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_MINIMUM_TIME_BETWEEN_DELETES, HealthReportConstants.DEFAULT_MINIMUM_TIME_BETWEEN_DELETES);
  }
}
