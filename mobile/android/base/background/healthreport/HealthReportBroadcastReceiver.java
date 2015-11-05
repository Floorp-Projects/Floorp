/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.background.common.log.Logger;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Watch for internal notifications to start Health Report background services.
 */
public class HealthReportBroadcastReceiver extends BroadcastReceiver {
  public static final String LOG_TAG = HealthReportBroadcastReceiver.class.getSimpleName();

  /**
   * Forward the intent (action and extras) to an IntentService to do background processing.
   */
  @Override
  public void onReceive(Context context, Intent intent) {
    Logger.debug(LOG_TAG, "Received intent - forwarding to BroadcastService.");
    Intent service = new Intent(context, HealthReportBroadcastService.class);
    // It's safe to forward extras since these are internal intents.
    service.putExtras(intent);
    service.setAction(intent.getAction());
    context.startService(service);
  }
}
