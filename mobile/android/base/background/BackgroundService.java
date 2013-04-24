/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background;

import org.mozilla.gecko.background.common.log.Logger;

import android.app.AlarmManager;
import android.app.IntentService;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Build;

public abstract class BackgroundService extends IntentService {
  private static final String LOG_TAG = BackgroundService.class.getSimpleName();

  protected BackgroundService() {
    super(LOG_TAG);
  }

  protected BackgroundService(String threadName) {
    super(threadName);
  }

  public static void runIntentInService(Context context, Intent intent, Class<? extends BackgroundService> serviceClass) {
    Intent service = new Intent(context, serviceClass);
    service.setAction(intent.getAction());
    service.putExtras(intent);
    context.startService(service);
  }

  /**
   * Returns true if the OS will allow us to perform background
   * data operations. This logic varies by OS version.
   */
  protected boolean backgroundDataIsEnabled() {
    ConnectivityManager connectivity = (ConnectivityManager) this.getSystemService(Context.CONNECTIVITY_SERVICE);
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
      return connectivity.getBackgroundDataSetting();
    }
    NetworkInfo networkInfo = connectivity.getActiveNetworkInfo();
    if (networkInfo == null) {
      return false;
    }
    return networkInfo.isAvailable();
  }

  protected static PendingIntent createPendingIntent(Context context, Class<? extends BroadcastReceiver> broadcastReceiverClass) {
    final Intent service = new Intent(context, broadcastReceiverClass);
    return PendingIntent.getBroadcast(context, 0, service, PendingIntent.FLAG_CANCEL_CURRENT);
  }

  protected AlarmManager getAlarmManager() {
    return getAlarmManager(this.getApplicationContext());
  }

  protected static AlarmManager getAlarmManager(Context context) {
    return (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
  }

  protected void scheduleAlarm(long pollInterval, PendingIntent pendingIntent) {
    Logger.info(LOG_TAG, "Setting inexact repeating alarm for interval " + pollInterval);
    if (pollInterval <= 0) {
        throw new IllegalArgumentException("pollInterval " + pollInterval + " must be positive");
    }
    final AlarmManager alarm = getAlarmManager();
    final long firstEvent = System.currentTimeMillis();
    alarm.setInexactRepeating(AlarmManager.RTC, firstEvent, pollInterval, pendingIntent);
  }

  protected void cancelAlarm(PendingIntent pendingIntent) {
    final AlarmManager alarm = getAlarmManager();
    alarm.cancel(pendingIntent);
  }
}
