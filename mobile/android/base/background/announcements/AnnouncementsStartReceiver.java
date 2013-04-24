/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.announcements;

import org.mozilla.gecko.background.BackgroundService;
import org.mozilla.gecko.background.common.log.Logger;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Start the announcements service when instructed by the {@link android.app.AlarmManager}.
 */
public class AnnouncementsStartReceiver extends BroadcastReceiver {

  private static final String LOG_TAG = "AnnounceStartRec";

  @Override
  public void onReceive(Context context, Intent intent) {
    if (AnnouncementsConstants.DISABLED) {
      return;
    }

    Logger.debug(LOG_TAG, "AnnouncementsStartReceiver.onReceive().");
    BackgroundService.runIntentInService(context, intent, AnnouncementsService.class);
  }
}
