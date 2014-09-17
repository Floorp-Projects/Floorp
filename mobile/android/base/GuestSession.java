package org.mozilla.gecko;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

// Utility methods for entering/exiting guest mode.
public class GuestSession {
    public static final String NOTIFICATION_INTENT = "org.mozilla.gecko.GUEST_SESSION_INPROGRESS";

    private static PendingIntent getNotificationIntent(Context context) {
        Intent intent = new Intent(NOTIFICATION_INTENT);
        return PendingIntent.getActivity(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    public static void showNotification(Context context) {
        final NotificationCompat.Builder builder = new NotificationCompat.Builder(context);
        final Resources res = context.getResources();
        builder.setContentTitle(res.getString(R.string.guest_browsing_notification_title))
               .setContentText(res.getString(R.string.guest_browsing_notification_text))
               .setSmallIcon(R.drawable.alert_guest)
               .setOngoing(true)
               .setContentIntent(getNotificationIntent(context));

        final NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.notify(R.id.guestNotification, builder.build());
    }

    public static void hideNotification(Context context) {
        final NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(R.id.guestNotification);
    }

    public static void onDestroy(Context context) {
        if (GeckoProfile.get(context).inGuestMode()) {
            hideNotification(context);
        }
    }

    public static void handleIntent(BrowserApp context, Intent intent) {
        context.showGuestModeDialog(BrowserApp.GuestModeDialog.LEAVING);
    }

}
