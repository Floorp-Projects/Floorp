/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.text.TextUtils;
import android.util.Log;
import com.keepsafe.switchboard.SwitchBoard;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.Experiments;

import java.util.Locale;

public class WhatsNewReceiver extends BroadcastReceiver {

    public static final String EXTRA_WHATSNEW_NOTIFICATION = "whatsnew_notification";
    private static final String ACTION_NOTIFICATION_CANCELLED = "notification_cancelled";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (ACTION_NOTIFICATION_CANCELLED.equals(intent.getAction())) {
            Telemetry.sendUIEvent(TelemetryContract.Event.CANCEL, TelemetryContract.Method.NOTIFICATION, EXTRA_WHATSNEW_NOTIFICATION);
            return;
        }

        final String dataString = intent.getDataString();
        if (TextUtils.isEmpty(dataString) || !dataString.contains(AppConstants.ANDROID_PACKAGE_NAME)) {
            return;
        }

        if (!SwitchBoard.isInExperiment(context, Experiments.WHATSNEW_NOTIFICATION)) {
            return;
        }

        if (!isPreferenceEnabled(context)) {
            return;
        }

        showWhatsNewNotification(context);
    }

    private boolean isPreferenceEnabled(Context context) {
        return GeckoSharedPrefs.forApp(context).getBoolean(GeckoPreferences.PREFS_NOTIFICATIONS_WHATS_NEW, true);
    }

    private void showWhatsNewNotification(Context context) {
        final Notification notification = new NotificationCompat.Builder(context)
                .setContentTitle(context.getString(R.string.whatsnew_notification_title))
                .setContentText(context.getString(R.string.whatsnew_notification_summary))
                .setSmallIcon(R.drawable.ic_status_logo)
                .setAutoCancel(true)
                .setContentIntent(getContentIntent(context))
                .setDeleteIntent(getDeleteIntent(context))
                .build();

        final NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        final int notificationID = EXTRA_WHATSNEW_NOTIFICATION.hashCode();
        notificationManager.notify(notificationID, notification);

        Telemetry.sendUIEvent(TelemetryContract.Event.SHOW, TelemetryContract.Method.NOTIFICATION, EXTRA_WHATSNEW_NOTIFICATION);
    }

    private PendingIntent getContentIntent(Context context) {
        final String link = context.getString(R.string.whatsnew_notification_url,
            AppConstants.MOZ_APP_VERSION,
            AppConstants.OS_TARGET,
            Locales.getLanguageTag(Locale.getDefault()));

        final Intent i = new Intent(Intent.ACTION_VIEW);
        i.setClassName(AppConstants.ANDROID_PACKAGE_NAME, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        i.setData(Uri.parse(link));
        i.putExtra(EXTRA_WHATSNEW_NOTIFICATION, true);

        return PendingIntent.getActivity(context, 0, i, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntent getDeleteIntent(Context context) {
        final Intent i = new Intent(context, WhatsNewReceiver.class);
        i.setAction(ACTION_NOTIFICATION_CANCELLED);

        return PendingIntent.getBroadcast(context, 0, i, PendingIntent.FLAG_CANCEL_CURRENT);
    }
}
