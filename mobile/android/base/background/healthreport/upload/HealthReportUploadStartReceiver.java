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
 * Start the Health Report background upload service when instructed by the
 * {@link android.app.AlarmManager}.
 */
public class HealthReportUploadStartReceiver extends BroadcastReceiver {
  public static final String LOG_TAG = HealthReportUploadStartReceiver.class.getSimpleName();

  @Override
  public void onReceive(Context context, Intent intent) {
    if (HealthReportConstants.UPLOAD_FEATURE_DISABLED) {
      Logger.debug(LOG_TAG, "Health report upload feature is compile-time disabled; not starting background upload service.");
      return;
    }

    Logger.debug(LOG_TAG, "Health report upload feature is compile-time enabled; starting background upload service.");
    Intent service = new Intent(context, HealthReportUploadService.class);
    service.setAction(intent.getAction());
    service.putExtras(intent); // profileName, profilePath, uploadEnabled are in the extras.
    context.startService(service);
  }
}
