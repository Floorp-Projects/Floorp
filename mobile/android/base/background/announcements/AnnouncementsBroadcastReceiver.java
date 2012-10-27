/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Watch for notifications to start the announcements service.
 *
 * Some observations:
 *
 * "Also note that as of Android 3.0 the user needs to have started the
 *  application at least once before your application can receive
 *  android.intent.action.BOOT_COMPLETED events."
 */
public class AnnouncementsBroadcastReceiver extends BroadcastReceiver {

  /**
   * Forward the intent to an IntentService to do background processing.
   */
  @Override
  public void onReceive(Context context, Intent intent) {
    if (AnnouncementsConstants.DISABLED) {
      return;
    }

    Intent service = new Intent(context, AnnouncementsBroadcastService.class);
    service.putExtras(intent);
    service.setAction(intent.getAction());
    context.startService(service);
  }
}
