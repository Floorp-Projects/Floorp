/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.mozglue.SafeIntent;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

/**
 *  Broadcast receiver for Notifications. Will forward them to GeckoApp (and start Gecko) if they're clicked.
 *  If they're being dismissed, it will not start Gecko, but may forward them to JS if Gecko is running.
 *  This is also the only entry point for notification intents.
 */
public class NotificationReceiver extends BroadcastReceiver {
    private static final String LOGTAG = "Gecko" + NotificationReceiver.class.getSimpleName();

    public void onReceive(Context context, Intent intent) {
        final Uri data = intent.getData();
        if (data == null) {
            Log.e(LOGTAG, "handleNotificationEvent: empty data");
            return;
        }

        final String notificationType = data.getQueryParameter(NotificationHelper.EVENT_TYPE_ATTR);
        if (notificationType == null) {
            return;
        }

        // In case the user swiped out the notification, we empty the id set.
        if (NotificationHelper.CLEARED_EVENT.equals(notificationType)) {
            // If Gecko isn't running, we throw away events where the notification was cancelled.
            // i.e. Don't bug the user if they're just closing a bunch of notifications.
            if (GeckoThread.isRunning()) {
                NotificationHelper.getArgsAndSendNotificationIntent(new SafeIntent(intent));
            }
            return;
        }

        forwardMessageToActivity(intent, context);
    }

    private void forwardMessageToActivity(final Intent intent, final Context context) {
        final ComponentName name = intent.getExtras().getParcelable(NotificationHelper.ORIGINAL_EXTRA_COMPONENT);
        intent.setComponent(name);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }
}