/* vim: set ts=4 sw=4 tw=78 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
package org.mozilla.b2gdroid;

import java.io.ByteArrayOutputStream;
import java.util.Map;
import java.util.HashMap;
import java.util.Arrays;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;

import android.app.PendingIntent;
import android.app.Notification;
import android.app.NotificationManager;

import android.service.notification.NotificationListenerService;
import android.service.notification.NotificationListenerService.RankingMap;
import android.service.notification.StatusBarNotification;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import android.util.Base64;
import android.util.Log;

import org.json.JSONObject;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.GeckoEventListener;

public class NotificationObserver extends NotificationListenerService
                                  implements GeckoEventListener {

    private static final String LOGTAG = "B2GDroid:NotificationObserver";

    private static final String ACTION_REGISTER_FOR_NOTIFICATIONS = AppConstants.ANDROID_PACKAGE_NAME + ".ACTION_REGISTER_FOR_NOTIFICATIONS";

    // Map a unique identifier consisting of the notification's
    // tag and id.  If the notification does not have a tag, the notification
    // is keyed on the package name and id to attempt to namespace it
    private Map<String, StatusBarNotification> mCurrentNotifications = new HashMap<String, StatusBarNotification>();

    @Override
    public void onCreate() {
        Log.i(LOGTAG, "onCreate()");
        super.onCreate();

        RemoteGeckoEventProxy.registerRemoteGeckoThreadListener(this, this, "Android:NotificationOpened", "Android:NotificationClosed");
    }


    @Override
    public void onNotificationPosted(StatusBarNotification aNotification, RankingMap aRanking) {
        Log.i(LOGTAG, "onNotificationPosted(aNotification, aRanking)");
        this.onNotificationPosted(aNotification);
    }

    @Override
    public void onNotificationPosted(StatusBarNotification aNotification) {
        Log.i(LOGTAG, "onNotificationPosted(aNotification, aRanking)");

        Notification notification = aNotification.getNotification();

        CharSequence title = notification.extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence extraText = notification.extras.getCharSequence(Notification.EXTRA_TEXT);

        String notificationKey = getNotificationKey(aNotification);

        JSONObject notificationData = new JSONObject();
        JSONObject behaviors = new JSONObject();

        try {
            notificationData.put("_action", "post");
            notificationData.put("id", notificationKey);
            notificationData.put("title", title.toString());
            notificationData.put("text", extraText.toString());
            notificationData.put("manifestURL", "android://" + aNotification.getPackageName().toLowerCase() + "/manifest.webapp");
            notificationData.put("icon", getBase64Icon(aNotification));
            notificationData.put("timestamp", aNotification.getPostTime());

            if ((Notification.FLAG_ONGOING_EVENT & notification.flags) != 0) {
                notificationData.put("noNotify", true);
            }

            behaviors.put("vibrationPattern", Arrays.asList(notification.vibrate));

            if ((Notification.FLAG_NO_CLEAR & notification.flags) != 0) {
                behaviors.put("noclear", true);
            }

            notificationData.put("mozbehavior", behaviors);

        } catch(Exception ex) {
            Log.wtf(LOGTAG, "Error building android notification message " + ex.getMessage());
            return;
        }

        mCurrentNotifications.put(notificationKey, aNotification);
        sendBroadcast(GeckoEventReceiver.createBroadcastEventIntent("Android:Notification", notificationData.toString()));
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification aNotification, RankingMap aRanking) {
        Log.i(LOGTAG, "onNotifciationRemoved(aNotification, aRanking)");
        onNotificationRemoved(aNotification);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification aNotification) {
        Log.i(LOGTAG, "onNotificationRemoved(aNotification)");

        String notificationKey = getNotificationKey(aNotification);
        removeNotificationFromGaia(notificationKey);
    }

    private void removeNotificationFromGaia(String aNotificationKey) {
        Log.i(LOGTAG, "removeNotificationFromGaia(aNotificationKey=" + aNotificationKey + ")");
        JSONObject notificationData = new JSONObject();

        mCurrentNotifications.remove(aNotificationKey);

        try {
            notificationData.put("_action", "remove");
            notificationData.put("id", aNotificationKey);
        } catch(Exception ex) {
            Log.wtf(LOGTAG, "Error building android notification message " + ex.getMessage());
            return;
        }

        sendBroadcast(GeckoEventReceiver.createBroadcastEventIntent("Android:Notification", notificationData.toString()));
    }

    private String getNotificationKey(StatusBarNotification aNotification) {
        // API Level 20 has a 'getKey' method, however for backwards we
        // implement this getNotificationKey
        String notificationTag = aNotification.getTag();
        if (notificationTag == null) {
           notificationTag = aNotification.getPackageName();
        }

        return notificationTag + "#" + Integer.toString(aNotification.getId());
    }

    private String getBase64Icon(StatusBarNotification aStatusBarNotification) {
        Notification notification = aStatusBarNotification.getNotification();

        PackageManager manager = getPackageManager();
        Resources notifyingPackageResources;
        Bitmap bitmap = null;

        try {
            notifyingPackageResources = manager.getResourcesForApplication(aStatusBarNotification.getPackageName());
            int resourceId = notification.icon;
            bitmap = BitmapFactory.decodeResource(notifyingPackageResources, resourceId);
        } catch(NameNotFoundException ex) {
            Log.i(LOGTAG, "Could not find package name: " + ex.getMessage());
        }

        if (bitmap == null) {
            // Try and get the icon directly from the notification
            if (notification.largeIcon != null) {
                bitmap = notification.largeIcon;
            } else {
                Log.i(LOGTAG, "No image found for notification");
                return "";
            }
        }

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, baos);
        return "data:image/png;base64," + Base64.encodeToString(baos.toByteArray(), Base64.NO_WRAP);
    }

    /* GeckoEventListener */
    public void handleMessage(String aEvent, JSONObject message) {
        Log.i(LOGTAG, "handleMessage(aEvent=" + aEvent + ")");

        String notificationId;
        try {
            notificationId = message.getString("id");
        } catch(Exception ex) {
            Log.i(LOGTAG, "Malformed Android:NotificationOpened JSON - missing ID");
            return;
        }

        StatusBarNotification sbNotification = mCurrentNotifications.get(notificationId);

        if (sbNotification == null) {
            Log.w(LOGTAG, "No notification found for ID: " + notificationId);
            return;
        }

        if ("Android:NotificationOpened".equals(aEvent)) {
            PendingIntent contentIntent = sbNotification.getNotification().contentIntent;

            try {
                contentIntent.send();
            } catch(Exception ex) {
                Log.i(LOGTAG, "Intent was cancelled");
            }

            removeNotificationFromGaia(notificationId);
            return;
        } else if("Android:NotificationClose".equals(aEvent)) {
            NotificationManager notificationManager = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);

            notificationManager.cancel(sbNotification.getTag(), sbNotification.getId());
            mCurrentNotifications.remove(notificationId);
        }
    }

    public static void registerForNativeNotifications(Context context) {
        Intent intent = createNotificationIntent(context, ACTION_REGISTER_FOR_NOTIFICATIONS);
        context.startService(intent);
    }

    private static Intent createNotificationIntent(Context context, String action) {
        return new Intent(action, /* URI */ null, context, NotificationObserver.class);
    }
}
