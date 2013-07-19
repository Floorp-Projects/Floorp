/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * A service which listens to broadcast intents from the system and from the
 * browser, registering or unregistering the main
 * {@link AnnouncementsStartReceiver} with the {@link AlarmManager}.
 */
public class AnnouncementsBroadcastService extends BackgroundService {
  private static final String WORKER_THREAD_NAME = "AnnouncementsBroadcastServiceWorker";
  private static final String LOG_TAG = "AnnounceBrSvc";

  public AnnouncementsBroadcastService() {
    super(WORKER_THREAD_NAME);
  }

  private void toggleAlarm(final Context context, boolean enabled) {
    Logger.info(LOG_TAG, (enabled ? "R" : "Unr") + "egistering announcements broadcast receiver...");

    final PendingIntent pending = createPendingIntent(context, AnnouncementsStartReceiver.class);

    if (!enabled) {
      cancelAlarm(pending);
      return;
    }

    final long pollInterval = getPollInterval(context);
    scheduleAlarm(pollInterval, pending);
  }

  /**
   * Record the last launch time of our version of Fennec.
   *
   * @param context
   *          the <code>Context</code> to use to gain access to
   *          <code>SharedPreferences</code>.
   */
  public static void recordLastLaunch(final Context context) {
    final long now = System.currentTimeMillis();
    final SharedPreferences preferences = context.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, GlobalConstants.SHARED_PREFERENCES_MODE);

    // One of several things might be true, according to our logs:
    //
    // * The new current time is older than the last
    // * … or way in the future
    // * … or way in the distant past
    // * … or it's reasonable.
    //
    // Furthermore, when we come to calculate idle we might find that the clock
    // is dramatically different — that the current time is thirteen years older
    // than our saved timestamp (system clock resets to 2000 on battery change),
    // or it's thirty years in the future (previous timestamp was saved as 0).
    //
    // We should try to do something vaguely sane in these situations.
    long previous = preferences.getLong(AnnouncementsConstants.PREF_LAST_LAUNCH, -1);
    if (previous == -1) {
      Logger.debug(LOG_TAG, "No previous launch recorded.");
    }

    if (now < GlobalConstants.BUILD_TIMESTAMP_MSEC) {
      Logger.warn(LOG_TAG, "Current time " + now + " is older than build date " +
                           GlobalConstants.BUILD_TIMESTAMP_MSEC + ". Ignoring until clock is corrected.");
      return;
    }

    if (now > AnnouncementsConstants.LATEST_ACCEPTED_LAUNCH_TIMESTAMP_MSEC) {
      Logger.warn(LOG_TAG, "Launch time " + now + " is later than max sane launch timestamp " +
                           AnnouncementsConstants.LATEST_ACCEPTED_LAUNCH_TIMESTAMP_MSEC +
                           ". Ignoring until clock is corrected.");
      return;
    }

    if (previous > now) {
      Logger.debug(LOG_TAG, "Previous launch " + previous + " later than current time " +
                            now + ", but new time is sane. Accepting new time.");
    }

    preferences.edit().putLong(AnnouncementsConstants.PREF_LAST_LAUNCH, now).commit();
  }

  public static long getPollInterval(final Context context) {
    SharedPreferences preferences = context.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, GlobalConstants.SHARED_PREFERENCES_MODE);
    return preferences.getLong(AnnouncementsConstants.PREF_ANNOUNCE_FETCH_INTERVAL_MSEC, AnnouncementsConstants.DEFAULT_ANNOUNCE_FETCH_INTERVAL_MSEC);
  }

  public static void setPollInterval(final Context context, long interval) {
    SharedPreferences preferences = context.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, GlobalConstants.SHARED_PREFERENCES_MODE);
    preferences.edit().putLong(AnnouncementsConstants.PREF_ANNOUNCE_FETCH_INTERVAL_MSEC, interval).commit();
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    Logger.setThreadLogTag(AnnouncementsConstants.GLOBAL_LOG_TAG);
    final String action = intent.getAction();
    Logger.debug(LOG_TAG, "Broadcast onReceive. Intent is " + action);

    if (AnnouncementsConstants.ACTION_ANNOUNCEMENTS_PREF.equals(action)) {
      handlePrefIntent(intent);
      return;
    }

    if (Intent.ACTION_BOOT_COMPLETED.equals(action) ||
        Intent.ACTION_EXTERNAL_APPLICATIONS_AVAILABLE.equals(action)) {
      BackgroundService.reflectContextToFennec(this,
          GlobalConstants.GECKO_PREFERENCES_CLASS,
          GlobalConstants.GECKO_BROADCAST_ANNOUNCEMENTS_PREF_METHOD);
      return;
    }

    // Failure case.
    Logger.warn(LOG_TAG, "Unknown intent " + action);
  }

  /**
   * Handle the intent sent by the browser when it wishes to notify us
   * of the value of the user preference. Look at the value and toggle the
   * alarm service accordingly.
   */
  protected void handlePrefIntent(Intent intent) {
    if (!intent.hasExtra("enabled")) {
      Logger.warn(LOG_TAG, "Got ANNOUNCEMENTS_PREF intent without enabled. Ignoring.");
      return;
    }

    final boolean enabled = intent.getBooleanExtra("enabled", true);
    Logger.debug(LOG_TAG, intent.getStringExtra("branch") + "/" +
                          intent.getStringExtra("pref")   + " = " +
                          (intent.hasExtra("enabled") ? enabled : ""));

    toggleAlarm(this, enabled);

    // Primarily intended for debugging and testing, but this doesn't do any harm.
    if (!enabled) {
      Logger.info(LOG_TAG, "!enabled: clearing last fetch.");
      final SharedPreferences sharedPreferences = this.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH,
                                                                            GlobalConstants.SHARED_PREFERENCES_MODE);
      final Editor editor = sharedPreferences.edit();
      editor.remove(AnnouncementsConstants.PREF_LAST_FETCH_LOCAL_TIME);
      editor.remove(AnnouncementsConstants.PREF_EARLIEST_NEXT_ANNOUNCE_FETCH);
      editor.commit();
    }
  }
}
