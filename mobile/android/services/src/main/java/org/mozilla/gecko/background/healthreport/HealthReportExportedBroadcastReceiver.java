/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import org.mozilla.gecko.background.common.log.Logger;

/**
 * Watch for external system notifications to start Health Report background services.
 *
 * Some observations:
 *
 * From the Android documentation: "Also note that as of Android 3.0 the user
 * needs to have started the application at least once before your application
 * can receive android.intent.action.BOOT_COMPLETED events."
 *
 * We really do want to launch on BOOT_COMPLETED, since it's possible for a user
 * to run Firefox, shut down the phone, then power it on again on the same day.
 * We want to submit a health report in this case, even though they haven't
 * launched Firefox since boot.
 */
public class HealthReportExportedBroadcastReceiver extends BroadcastReceiver {
  public static final String LOG_TAG = HealthReportExportedBroadcastReceiver.class.getSimpleName();

  /**
   * Forward the intent action to an IntentService to do background processing.
   * We intentionally do not forward extras, since there are none needed from
   * external events.
   */
  @Override
  public void onReceive(Context context, Intent intent) {
    Logger.debug(LOG_TAG, "Received intent - forwarding to BroadcastService.");
    final Intent service = new Intent(context, HealthReportBroadcastService.class);
    // We intentionally copy only the intent action.
    service.setAction(intent.getAction());
    context.startService(service);
  }
}
