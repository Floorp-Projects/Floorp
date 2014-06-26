/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.prune.HealthReportPruneService;
import org.mozilla.gecko.background.healthreport.upload.HealthReportUploadService;
import org.mozilla.gecko.background.healthreport.upload.ObsoleteDocumentTracker;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * A service which listens to broadcast intents from the system and from the
 * browser, registering or unregistering the background health report services with the
 * {@link AlarmManager}.
 */
public class HealthReportBroadcastService extends BackgroundService {
  public static final String LOG_TAG = HealthReportBroadcastService.class.getSimpleName();
  public static final String WORKER_THREAD_NAME = LOG_TAG + "Worker";

  public HealthReportBroadcastService() {
    super(WORKER_THREAD_NAME);
  }

  protected SharedPreferences getSharedPreferences() {
    return this.getSharedPreferences(HealthReportConstants.PREFS_BRANCH, GlobalConstants.SHARED_PREFERENCES_MODE);
  }

  public long getSubmissionPollInterval() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_SUBMISSION_INTENT_INTERVAL_MSEC, HealthReportConstants.DEFAULT_SUBMISSION_INTENT_INTERVAL_MSEC);
  }

  public void setSubmissionPollInterval(final long interval) {
    getSharedPreferences().edit().putLong(HealthReportConstants.PREF_SUBMISSION_INTENT_INTERVAL_MSEC, interval).commit();
  }

  public long getPrunePollInterval() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_PRUNE_INTENT_INTERVAL_MSEC,
        HealthReportConstants.DEFAULT_PRUNE_INTENT_INTERVAL_MSEC);
  }

  public void setPrunePollInterval(final long interval) {
    getSharedPreferences().edit().putLong(HealthReportConstants.PREF_PRUNE_INTENT_INTERVAL_MSEC,
        interval).commit();
  }

  /**
   * Set or cancel an alarm to submit data for a profile.
   *
   * @param context
   *          Android context.
   * @param profileName
   *          to submit data for.
   * @param profilePath
   *          to submit data for.
   * @param enabled
   *          whether the user has enabled submitting health report data for
   *          this profile.
   * @param serviceEnabled
   *          whether submitting should be scheduled. If the user turns off
   *          submitting, <code>enabled</code> could be false but we could need
   *          to delete so <code>serviceEnabled</code> could be true.
   */
  protected void toggleSubmissionAlarm(final Context context, String profileName, String profilePath,
      boolean enabled, boolean serviceEnabled) {
    final Class<?> serviceClass = HealthReportUploadService.class;
    Logger.info(LOG_TAG, (serviceEnabled ? "R" : "Unr") + "egistering " +
        serviceClass.getSimpleName() + ".");

    // PendingIntents are compared without reference to their extras. Therefore
    // even though we pass the profile details to the action, different
    // profiles will share the *same* pending intent. In a multi-profile future,
    // this will need to be addressed.  See Bug 882182.
    final Intent service = new Intent(context, serviceClass);
    service.setAction("upload"); // PendingIntents "lose" their extras if no action is set.
    service.putExtra("uploadEnabled", enabled);
    service.putExtra("profileName", profileName);
    service.putExtra("profilePath", profilePath);
    final PendingIntent pending = PendingIntent.getService(context, 0, service, PendingIntent.FLAG_CANCEL_CURRENT);

    if (!serviceEnabled) {
      cancelAlarm(pending);
      return;
    }

    final long pollInterval = getSubmissionPollInterval();
    scheduleAlarm(pollInterval, pending);
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    Logger.setThreadLogTag(HealthReportConstants.GLOBAL_LOG_TAG);

    // Intent can be null. Bug 1025937.
    if (intent == null) {
      Logger.debug(LOG_TAG, "Short-circuiting on null intent.");
      return;
    }

    // The same intent can be handled by multiple methods so do not short-circuit evaluate.
    boolean handled = attemptHandleIntentForUpload(intent);
    handled = attemptHandleIntentForPrune(intent) ? true : handled;

    if (!handled) {
      Logger.warn(LOG_TAG, "Unhandled intent with action " + intent.getAction() + ".");
    }
  }

  /**
   * Attempts to handle the given intent for FHR document upload. If it cannot, false is returned.
   *
   * @param intent must be non-null.
   */
  private boolean attemptHandleIntentForUpload(final Intent intent) {
    if (HealthReportConstants.UPLOAD_FEATURE_DISABLED) {
      Logger.debug(LOG_TAG, "Health report upload feature is compile-time disabled; not handling intent.");
      return false;
    }

    final String action = intent.getAction();
    Logger.debug(LOG_TAG, "Health report upload feature is compile-time enabled; attempting to " +
        "handle intent with action " + action + ".");

    if (HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF.equals(action)) {
      handleUploadPrefIntent(intent);
      return true;
    }

    if (Intent.ACTION_BOOT_COMPLETED.equals(action) ||
        Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE.equals(action)) {
      BackgroundService.reflectContextToFennec(this,
          GlobalConstants.GECKO_PREFERENCES_CLASS,
          GlobalConstants.GECKO_BROADCAST_HEALTHREPORT_UPLOAD_PREF_METHOD);
      return true;
    }

    return false;
  }

  /**
   * Handle the intent sent by the browser when it wishes to notify us
   * of the value of the user preference. Look at the value and toggle the
   * alarm service accordingly.
   *
   * @param intent must be non-null.
   */
  private void handleUploadPrefIntent(Intent intent) {
    if (!intent.hasExtra("enabled")) {
      Logger.warn(LOG_TAG, "Got " + HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF + " intent without enabled. Ignoring.");
      return;
    }

    final boolean enabled = intent.getBooleanExtra("enabled", true);
    Logger.debug(LOG_TAG, intent.getStringExtra("branch") + "/" +
                          intent.getStringExtra("pref")   + " = " +
                          (intent.hasExtra("enabled") ? enabled : ""));

    String profileName = intent.getStringExtra("profileName");
    String profilePath = intent.getStringExtra("profilePath");

    if (profileName == null || profilePath == null) {
      Logger.warn(LOG_TAG, "Got " + HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF + " intent without profilePath or profileName. Ignoring.");
      return;
    }

    Logger.pii(LOG_TAG, "Updating health report upload alarm for profile " + profileName + " at " +
        profilePath + ".");

    final SharedPreferences sharedPrefs = getSharedPreferences();
    final ObsoleteDocumentTracker tracker = new ObsoleteDocumentTracker(sharedPrefs);
    final boolean hasObsoleteIds = tracker.hasObsoleteIds();

    if (!enabled) {
      final Editor editor = sharedPrefs.edit();
      editor.remove(HealthReportConstants.PREF_LAST_UPLOAD_DOCUMENT_ID);

      if (hasObsoleteIds) {
        Logger.debug(LOG_TAG, "Health report upload disabled; scheduling deletion of " + tracker.numberOfObsoleteIds() + " documents.");
        tracker.limitObsoleteIds();
      } else {
        // Primarily intended for debugging and testing.
        Logger.debug(LOG_TAG, "Health report upload disabled and no deletes to schedule: clearing prefs.");
        editor.remove(HealthReportConstants.PREF_FIRST_RUN);
        editor.remove(HealthReportConstants.PREF_NEXT_SUBMISSION);
      }

      editor.commit();
    }

    // The user can toggle us off or on, or we can have obsolete documents to
    // remove.
    final boolean serviceEnabled = hasObsoleteIds || enabled;
    toggleSubmissionAlarm(this, profileName, profilePath, enabled, serviceEnabled);
  }

  /**
   * Attempts to handle the given intent for FHR data pruning. If it cannot, false is returned.
   *
   * @param intent must be non-null.
   */
  private boolean attemptHandleIntentForPrune(final Intent intent) {
    final String action = intent.getAction();
    Logger.debug(LOG_TAG, "Prune: Attempting to handle intent with action, " + action + ".");

    if (HealthReportConstants.ACTION_HEALTHREPORT_PRUNE.equals(action)) {
      handlePruneIntent(intent);
      return true;
    }

    if (Intent.ACTION_BOOT_COMPLETED.equals(action) ||
        Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE.equals(action)) {
      BackgroundService.reflectContextToFennec(this,
          GlobalConstants.GECKO_PREFERENCES_CLASS,
          GlobalConstants.GECKO_BROADCAST_HEALTHREPORT_PRUNE_METHOD);
      return true;
    }

    return false;
  }

  /**
   * @param intent must be non-null.
   */
  private void handlePruneIntent(final Intent intent) {
    final String profileName = intent.getStringExtra("profileName");
    final String profilePath = intent.getStringExtra("profilePath");

    if (profileName == null || profilePath == null) {
      Logger.warn(LOG_TAG, "Got " + HealthReportConstants.ACTION_HEALTHREPORT_PRUNE + " intent " +
          "without profilePath or profileName. Ignoring.");
      return;
    }

    final Class<?> serviceClass = HealthReportPruneService.class;
    final Intent service = new Intent(this, serviceClass);
    service.setAction("prune"); // Intents without actions have their extras removed.
    service.putExtra("profileName", profileName);
    service.putExtra("profilePath", profilePath);
    final PendingIntent pending = PendingIntent.getService(this, 0, service,
        PendingIntent.FLAG_CANCEL_CURRENT);

    // Set a regular alarm to start PruneService. Since the various actions that PruneService can
    // take occur on irregular intervals, we can be more efficient by only starting the Service
    // when one of these time limits runs out.  However, subsequent Service invocations must then
    // be registered by the PruneService itself, which would fail if the PruneService crashes.
    // Thus, we set this regular (and slightly inefficient) alarm.
    Logger.info(LOG_TAG, "Registering " + serviceClass.getSimpleName() + ".");
    final long pollInterval = getPrunePollInterval();
    scheduleAlarm(pollInterval, pending);
  }
}
