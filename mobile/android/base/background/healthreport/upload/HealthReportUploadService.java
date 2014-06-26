/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.upload;

import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.IBinder;

/**
 * A <code>Service</code> to manage and upload health report data.
 *
 * We extend <code>IntentService</code>, rather than just <code>Service</code>,
 * because this gives us a worker thread to avoid main-thread networking.
 *
 * Yes, even though we're in an alarm-triggered service, it still counts as
 * main-thread.
 */
public class HealthReportUploadService extends BackgroundService {
  public static final String LOG_TAG = HealthReportUploadService.class.getSimpleName();
  public static final String WORKER_THREAD_NAME = LOG_TAG + "Worker";

  public HealthReportUploadService() {
    super(WORKER_THREAD_NAME);
  }

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }

  protected SharedPreferences getSharedPreferences() {
    return this.getSharedPreferences(HealthReportConstants.PREFS_BRANCH, GlobalConstants.SHARED_PREFERENCES_MODE);
  }

  @Override
  public void onHandleIntent(Intent intent) {
    Logger.setThreadLogTag(HealthReportConstants.GLOBAL_LOG_TAG);

    // Intent can be null. Bug 1025937.
    if (intent == null) {
      Logger.debug(LOG_TAG, "Short-circuiting on null intent.");
      return;
    }

    if (HealthReportConstants.UPLOAD_FEATURE_DISABLED) {
      Logger.debug(LOG_TAG, "Health report upload feature is compile-time disabled; not handling upload intent.");
      return;
    }

    Logger.debug(LOG_TAG, "Health report upload feature is compile-time enabled; handling upload intent.");

    String profileName = intent.getStringExtra("profileName");
    String profilePath = intent.getStringExtra("profilePath");

    if (profileName == null || profilePath == null) {
      Logger.warn(LOG_TAG, "Got intent without profilePath or profileName. Ignoring.");
      return;
    }

    if (!intent.hasExtra("uploadEnabled")) {
      Logger.warn(LOG_TAG, "Got intent without uploadEnabled. Ignoring.");
      return;
    }
    boolean uploadEnabled = intent.getBooleanExtra("uploadEnabled", false);

    // Don't do anything if the device can't talk to the server.
    if (!backgroundDataIsEnabled()) {
      Logger.debug(LOG_TAG, "Background data is not enabled; skipping.");
      return;
    }

    Logger.pii(LOG_TAG, "Ticking policy for profile " + profileName + " at " + profilePath + ".");

    final SharedPreferences sharedPrefs = getSharedPreferences();
    final ObsoleteDocumentTracker tracker = new ObsoleteDocumentTracker(sharedPrefs);
    SubmissionClient client = new AndroidSubmissionClient(this, sharedPrefs, profilePath);
    SubmissionPolicy policy = new SubmissionPolicy(sharedPrefs, client, tracker, uploadEnabled);

    final long now = System.currentTimeMillis();
    policy.tick(now);
  }
}
