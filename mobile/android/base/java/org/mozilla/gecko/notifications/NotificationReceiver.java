/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoServicesCreatorService;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.mozglue.SafeIntent;

/**
 *  Broadcast receiver for Notifications. Will forward them to GeckoApp (and start Gecko) if they're clicked.
 *  If they're being dismissed, it will not start Gecko, but may forward them to JS if Gecko is running.
 *  This is also the only entry point for notification intents.
 */
public class NotificationReceiver extends BroadcastReceiver {
    private static final String LOGTAG = "GeckoNotificationReceiver";

    public void onReceive(Context context, Intent intent) {
        final Uri data = intent.getData();
        if (data == null) {
            Log.e(LOGTAG, "handleNotificationEvent: empty data");
            return;
        }

        final String action = intent.getAction();
        if (NotificationClient.CLICK_ACTION.equals(action) ||
                NotificationClient.CLOSE_ACTION.equals(action)) {
            onNotificationClientAction(context, action, data, intent);
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

            final NotificationClient client = (NotificationClient)
                    GeckoAppShell.getNotificationListener();
            client.onNotificationClose(data.getQueryParameter(NotificationHelper.ID_ATTR));
            return;
        }

        forwardMessageToActivity(intent, context);
    }

    private void forwardMessageToActivity(final Intent intent, final Context context) {
        final ComponentName name =
                intent.getExtras().getParcelable(NotificationHelper.ORIGINAL_EXTRA_COMPONENT);

        if (!AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS.equals(
                name != null ? name.getClassName() : null)) {
            // Don't try to start anything other than the browser Activity.
            NotificationHelper.getInstance(context.getApplicationContext())
                              .handleNotificationIntent(new SafeIntent(intent));
            return;
        }

        intent.setComponent(name);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    private void onNotificationClientAction(final Context context, final String action,
                                            final Uri data, final Intent intent) {
        final String name = data.getQueryParameter("name");
        final String cookie = data.getQueryParameter("cookie");
        final Intent persistentIntent = (Intent)
                intent.getParcelableExtra(NotificationClient.PERSISTENT_INTENT_EXTRA);

        if (persistentIntent != null) {
            // Go through GeckoService for persistent notifications.
            GeckoServicesCreatorService.enqueueWork(context, intent);
        }

        if (NotificationClient.CLICK_ACTION.equals(action)) {
            GeckoAppShell.onNotificationClick(name, cookie);

            if (persistentIntent != null) {
                // Don't launch GeckoApp if it's a background persistent notification.
                return;
            }

            final Intent appIntent = new Intent(GeckoApp.ACTION_ALERT_CALLBACK);
            appIntent.setComponent(new ComponentName(
                    data.getAuthority(), data.getPath().substring(1))); // exclude leading slash.
            appIntent.setData(data);
            appIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(appIntent);

        } else if (NotificationClient.CLOSE_ACTION.equals(action)) {
            GeckoAppShell.onNotificationClose(name, cookie);

            final NotificationClient client = (NotificationClient)
                    GeckoAppShell.getNotificationListener();
            client.onNotificationClose(name);
        }
    }
}
