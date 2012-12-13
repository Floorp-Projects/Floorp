/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import org.mozilla.gecko.background.BackgroundConstants;
import org.mozilla.gecko.sync.Logger;

import android.app.AlarmManager;
import android.app.IntentService;
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
public class AnnouncementsBroadcastService extends IntentService {
  private static final String WORKER_THREAD_NAME = "AnnouncementsBroadcastServiceWorker";
  private static final String LOG_TAG = "AnnounceBrSvc";

  public AnnouncementsBroadcastService() {
    super(WORKER_THREAD_NAME);
  }

  private void toggleAlarm(final Context context, boolean enabled) {
    Logger.info(LOG_TAG, (enabled ? "R" : "Unr") + "egistering announcements broadcast receiver...");
    final AlarmManager alarm = getAlarmManager(context);

    final Intent service = new Intent(context, AnnouncementsStartReceiver.class);
    final PendingIntent pending = PendingIntent.getBroadcast(context, 0, service, PendingIntent.FLAG_CANCEL_CURRENT);

    if (!enabled) {
      alarm.cancel(pending);
      return;
    }

    final long firstEvent = System.currentTimeMillis();
    final long pollInterval = getPollInterval(context);
    Logger.info(LOG_TAG, "Setting inexact repeating alarm for interval " + pollInterval);
    alarm.setInexactRepeating(AlarmManager.RTC, firstEvent, pollInterval, pending);
  }

  private static AlarmManager getAlarmManager(Context context) {
    return (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
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
    final SharedPreferences preferences = context.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, BackgroundConstants.SHARED_PREFERENCES_MODE);
    preferences.edit().putLong(AnnouncementsConstants.PREF_LAST_LAUNCH, now).commit();
  }

  public static long getPollInterval(final Context context) {
    SharedPreferences preferences = context.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, BackgroundConstants.SHARED_PREFERENCES_MODE);
    return preferences.getLong(AnnouncementsConstants.PREF_ANNOUNCE_FETCH_INTERVAL_MSEC, AnnouncementsConstants.DEFAULT_ANNOUNCE_FETCH_INTERVAL_MSEC);
  }

  public static void setPollInterval(final Context context, long interval) {
    SharedPreferences preferences = context.getSharedPreferences(AnnouncementsConstants.PREFS_BRANCH, BackgroundConstants.SHARED_PREFERENCES_MODE);
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
      handleSystemLifetimeIntent();
      return;
    }

    // Failure case.
    Logger.warn(LOG_TAG, "Unknown intent " + action);
  }

  /**
   * Handle one of the system intents to which we listen to launch our service
   * without the browser being opened.
   *
   * To avoid tight coupling to Fennec, we use reflection to find
   * <code>GeckoPreferences</code>, invoking the same code path that
   * <code>GeckoApp</code> uses on startup to send the <i>other</i>
   * notification to which we listen.
   *
   * All of this is neatly wrapped in <code>try…catch</code>, so this code
   * will run safely without a Firefox build installed.
   */
  protected void handleSystemLifetimeIntent() {
    // Ask the browser to tell us the current state of the preference.
    try {
      Class<?> geckoPreferences = Class.forName(BackgroundConstants.GECKO_PREFERENCES_CLASS);
      Method broadcastSnippetsPref = geckoPreferences.getMethod(BackgroundConstants.GECKO_BROADCAST_METHOD, Context.class);
      broadcastSnippetsPref.invoke(null, this);
      return;
    } catch (ClassNotFoundException e) {
      Logger.error(LOG_TAG, "Class " + BackgroundConstants.GECKO_PREFERENCES_CLASS + " not found!");
      return;
    } catch (NoSuchMethodException e) {
      Logger.error(LOG_TAG, "Method " + BackgroundConstants.GECKO_PREFERENCES_CLASS + "/" + BackgroundConstants.GECKO_BROADCAST_METHOD + " not found!");
      return;
    } catch (IllegalArgumentException e) {
      // Fall through.
    } catch (IllegalAccessException e) {
      // Fall through.
    } catch (InvocationTargetException e) {
      // Fall through.
    }
    Logger.error(LOG_TAG, "Got exception invoking " + BackgroundConstants.GECKO_BROADCAST_METHOD + ".");
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
                                                                            BackgroundConstants.SHARED_PREFERENCES_MODE);
      final Editor editor = sharedPreferences.edit();
      editor.remove(AnnouncementsConstants.PREF_LAST_FETCH_LOCAL_TIME);
      editor.remove(AnnouncementsConstants.PREF_EARLIEST_NEXT_ANNOUNCE_FETCH);
      editor.commit();
    }
  }
}