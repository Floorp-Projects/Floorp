/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.upload;

import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.healthreport.HealthReportUtils;
import org.mozilla.gecko.sync.ExtendedJSONObject;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * A service which listens to broadcast intents from the system and from the
 * browser, registering or unregistering the main
 * {@link HealthReportUploadStartReceiver} with the {@link AlarmManager}.
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

  public long getPollInterval() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_SUBMISSION_INTENT_INTERVAL_MSEC, HealthReportConstants.DEFAULT_SUBMISSION_INTENT_INTERVAL_MSEC);
  }

  public void setPollInterval(long interval) {
    getSharedPreferences().edit().putLong(HealthReportConstants.PREF_SUBMISSION_INTENT_INTERVAL_MSEC, interval).commit();
  }

  public long getDeletionAttemptsPerObsoleteDocumentId() {
    return getSharedPreferences().getLong(HealthReportConstants.PREF_DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID, HealthReportConstants.DEFAULT_DELETION_ATTEMPTS_PER_OBSOLETE_DOCUMENT_ID);
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
  protected void toggleAlarm(final Context context, String profileName, String profilePath, boolean enabled, boolean serviceEnabled) {
    Logger.info(LOG_TAG, (serviceEnabled ? "R" : "Unr") + "egistering health report start broadcast receiver.");

    // PendingIntents are compared without reference to their extras. Therefore
    // even though we pass the profile details to the action, different
    // profiles will share the *same* pending intent. In a multi-profile future,
    // this will need to be addressed.  See Bug 882182.
    final Intent service = new Intent(context, HealthReportUploadStartReceiver.class);
    service.setAction("upload"); // PendingIntents "lose" their extras if no action is set.
    service.putExtra("uploadEnabled", enabled);
    service.putExtra("profileName", profileName);
    service.putExtra("profilePath", profilePath);
    final PendingIntent pending = PendingIntent.getBroadcast(context, 0, service, PendingIntent.FLAG_CANCEL_CURRENT);

    if (!serviceEnabled) {
      cancelAlarm(pending);
      return;
    }

    final long pollInterval = getPollInterval();
    scheduleAlarm(pollInterval, pending);
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    Logger.setThreadLogTag(HealthReportConstants.GLOBAL_LOG_TAG);

    if (HealthReportConstants.UPLOAD_FEATURE_DISABLED) {
      Logger.debug(LOG_TAG, "Health report upload feature is compile-time disabled; not handling intent.");
      return;
    }

    final String action = intent.getAction();
    Logger.debug(LOG_TAG, "Health report upload feature is compile-time enabled; handling intent with action " + action + ".");

    if (HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF.equals(action)) {
      handlePrefIntent(intent);
      return;
    }

    if (Intent.ACTION_BOOT_COMPLETED.equals(action) ||
        Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE.equals(action)) {
      BackgroundService.reflectContextToFennec(this,
          GlobalConstants.GECKO_PREFERENCES_CLASS,
          GlobalConstants.GECKO_BROADCAST_HEALTHREPORT_UPLOAD_PREF_METHOD);
      return;
    }

    // Failure case.
    Logger.warn(LOG_TAG, "Unknown intent " + action + ".");
  }

  /**
   * Handle the intent sent by the browser when it wishes to notify us
   * of the value of the user preference. Look at the value and toggle the
   * alarm service accordingly.
   */
  protected void handlePrefIntent(Intent intent) {
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

    Logger.pii(LOG_TAG, "Updating health report alarm for profile " + profileName + " at " + profilePath + ".");

    final SharedPreferences sharedPrefs = getSharedPreferences();

    ExtendedJSONObject obsoleteIds = HealthReportUtils.getObsoleteIds(getSharedPreferences());

    if (!enabled) {
      String lastId = sharedPrefs.getString(HealthReportConstants.PREF_LAST_UPLOAD_DOCUMENT_ID, null);
      final Editor editor = sharedPrefs.edit();
      editor.remove(HealthReportConstants.PREF_LAST_UPLOAD_DOCUMENT_ID);

      if (lastId != null) {
        try {
          obsoleteIds.put(lastId, getDeletionAttemptsPerObsoleteDocumentId());
        } catch (Exception e) {
          Logger.warn(LOG_TAG, "Got exception updating obsolete ids JSON.", e);
        }
        HealthReportUtils.setObsoleteIds(getSharedPreferences(), obsoleteIds);
        Logger.debug(LOG_TAG, "New health report to obsolete; scheduling deletion of " + obsoleteIds.size() + " documents.");
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
    final boolean serviceEnabled = (obsoleteIds.size() > 0) || enabled;
    toggleAlarm(this, profileName, profilePath, enabled, serviceEnabled);
  }
}
