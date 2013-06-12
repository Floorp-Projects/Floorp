/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport.upload;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Watch for notifications to start the Health Report background upload service.
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
public class HealthReportBroadcastReceiver extends BroadcastReceiver {
  public static final String LOG_TAG = HealthReportBroadcastReceiver.class.getSimpleName();

  /**
   * Forward the intent to an IntentService to do background processing.
   */
  @Override
  public void onReceive(Context context, Intent intent) {
    if (HealthReportConstants.UPLOAD_FEATURE_DISABLED) {
      Logger.debug(LOG_TAG, "Health report upload feature is compile-time disabled; not forwarding intent.");
      return;
    }

    Logger.debug(LOG_TAG, "Health report upload feature is compile-time enabled; forwarding intent.");
    Intent service = new Intent(context, HealthReportBroadcastService.class);
    service.putExtras(intent);
    service.setAction(intent.getAction());
    context.startService(service);
  }
}
