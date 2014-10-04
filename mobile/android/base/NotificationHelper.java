/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.StringUtils;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

import java.util.Iterator;
import java.util.HashMap;

public final class NotificationHelper implements GeckoEventListener {
    public static final String HELPER_BROADCAST_ACTION = AppConstants.ANDROID_PACKAGE_NAME + ".helperBroadcastAction";

    public static final String NOTIFICATION_ID = "NotificationHelper_ID";
    private static final String LOGTAG = "GeckoNotificationHelper";
    private static final String HELPER_NOTIFICATION = "helperNotif";

    // Attributes mandatory to be used while sending a notification from js.
    private static final String TITLE_ATTR = "title";
    private static final String TEXT_ATTR = "text";
    private static final String ID_ATTR = "id";
    private static final String SMALLICON_ATTR = "smallIcon";

    // Attributes that can be used while sending a notification from js.
    private static final String PROGRESS_VALUE_ATTR = "progress_value";
    private static final String PROGRESS_MAX_ATTR = "progress_max";
    private static final String PROGRESS_INDETERMINATE_ATTR = "progress_indeterminate";
    private static final String LIGHT_ATTR = "light";
    private static final String ONGOING_ATTR = "ongoing";
    private static final String WHEN_ATTR = "when";
    private static final String PRIORITY_ATTR = "priority";
    private static final String LARGE_ICON_ATTR = "largeIcon";
    private static final String EVENT_TYPE_ATTR = "eventType";
    private static final String ACTIONS_ATTR = "actions";
    private static final String ACTION_ID_ATTR = "buttonId";
    private static final String ACTION_TITLE_ATTR = "title";
    private static final String ACTION_ICON_ATTR = "icon";
    private static final String PERSISTENT_ATTR = "persistent";
    private static final String HANDLER_ATTR = "handlerKey";
    private static final String COOKIE_ATTR = "cookie";

    private static final String NOTIFICATION_SCHEME = "moz-notification";

    private static final String BUTTON_EVENT = "notification-button-clicked";
    private static final String CLICK_EVENT = "notification-clicked";
    private static final String CLEARED_EVENT = "notification-cleared";
    private static final String CLOSED_EVENT = "notification-closed";

    private Context mContext;

    // Holds a list of notifications that should be cleared if the Fennec Activity is shut down.
    // Will not include ongoing or persistent notifications that are tied to Gecko's lifecycle.
    private HashMap<String, String> mClearableNotifications;

    private boolean mInitialized = false;
    private static NotificationHelper sInstance;

    private NotificationHelper(Context context) {
        mContext = context;
    }

    public void init() {
        mClearableNotifications = new HashMap<String, String>();
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Notification:Show",
            "Notification:Hide");
        mInitialized = true;
    }

    public static NotificationHelper getInstance(Context context) {
        // If someone else created this singleton, but didn't initialize it, something has gone wrong.
        if (sInstance != null && !sInstance.mInitialized) {
            throw new IllegalStateException("NotificationHelper was created by someone else but not initialized");
        }

        if (sInstance == null) {
            sInstance = new NotificationHelper(context.getApplicationContext());
        }
        return sInstance;
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        if (event.equals("Notification:Show")) {
            showNotification(message);
        } else if (event.equals("Notification:Hide")) {
            hideNotification(message);
        }
    }

    public boolean isHelperIntent(Intent i) {
        return i.getBooleanExtra(HELPER_NOTIFICATION, false);
    }

    public void handleNotificationIntent(Intent i) {
        final Uri data = i.getData();
        if (data == null) {
            Log.e(LOGTAG, "handleNotificationEvent: empty data");
            return;
        }
        final String id = data.getQueryParameter(ID_ATTR);
        final String notificationType = data.getQueryParameter(EVENT_TYPE_ATTR);
        if (id == null || notificationType == null) {
            Log.e(LOGTAG, "handleNotificationEvent: invalid intent parameters");
            return;
        }

        // In case the user swiped out the notification, we empty the id set.
        if (CLEARED_EVENT.equals(notificationType)) {
            mClearableNotifications.remove(id);
            // If Gecko isn't running, we throw away events where the notification was cancelled.
            // i.e. Don't bug the user if they're just closing a bunch of notifications.
            if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
                return;
            }
        }

        JSONObject args = new JSONObject();

        // The handler and cookie parameters are optional.
        final String handler = data.getQueryParameter(HANDLER_ATTR);
        final String cookie = StringUtils.getStringExtra(i, COOKIE_ATTR);

        try {
            args.put(ID_ATTR, id);
            args.put(EVENT_TYPE_ATTR, notificationType);
            args.put(HANDLER_ATTR, handler);
            args.put(COOKIE_ATTR, cookie);

            if (BUTTON_EVENT.equals(notificationType)) {
                final String actionName = data.getQueryParameter(ACTION_ID_ATTR);
                args.put(ACTION_ID_ATTR, actionName);
            }

            Log.i(LOGTAG, "Send " + args.toString());
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Notification:Event", args.toString()));
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error building JSON notification arguments.", e);
        }

        // If the notification was clicked, we are closing it. This must be executed after
        // sending the event to js side because when the notification is canceled no event can be
        // handled.
        if (CLICK_EVENT.equals(notificationType) && !i.getBooleanExtra(ONGOING_ATTR, false)) {
            hideNotification(id, handler, cookie);
        }

    }

    private Uri.Builder getNotificationBuilder(JSONObject message, String type) {
        Uri.Builder b = new Uri.Builder();
        b.scheme(NOTIFICATION_SCHEME).appendQueryParameter(EVENT_TYPE_ATTR, type);

        try {
            final String id = message.getString(ID_ATTR);
            b.appendQueryParameter(ID_ATTR, id);
        } catch (JSONException ex) {
            Log.i(LOGTAG, "buildNotificationPendingIntent, error parsing", ex);
        }

        try {
            final String id = message.getString(HANDLER_ATTR);
            b.appendQueryParameter(HANDLER_ATTR, id);
        } catch (JSONException ex) {
            Log.i(LOGTAG, "Notification doesn't have a handler");
        }

        return b;
    }

    private Intent buildNotificationIntent(JSONObject message, Uri.Builder builder) {
        Intent notificationIntent = new Intent(HELPER_BROADCAST_ACTION);
        final boolean ongoing = message.optBoolean(ONGOING_ATTR);
        notificationIntent.putExtra(ONGOING_ATTR, ongoing);

        final Uri dataUri = builder.build();
        notificationIntent.setData(dataUri);
        notificationIntent.putExtra(HELPER_NOTIFICATION, true);
        notificationIntent.putExtra(COOKIE_ATTR, message.optString(COOKIE_ATTR));
        return notificationIntent;
    }

    private PendingIntent buildNotificationPendingIntent(JSONObject message, String type) {
        Uri.Builder builder = getNotificationBuilder(message, type);
        final Intent notificationIntent = buildNotificationIntent(message, builder);
        PendingIntent pi = PendingIntent.getActivity(mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        return pi;
    }

    private PendingIntent buildButtonClickPendingIntent(JSONObject message, JSONObject action) {
        Uri.Builder builder = getNotificationBuilder(message, BUTTON_EVENT);
        try {
            // Action name must be in query uri, otherwise buttons pending intents
            // would be collapsed.
            if(action.has(ACTION_ID_ATTR)) {
                builder.appendQueryParameter(ACTION_ID_ATTR, action.getString(ACTION_ID_ATTR));
            } else {
                Log.i(LOGTAG, "button event with no name");
            }
        } catch (JSONException ex) {
            Log.i(LOGTAG, "buildNotificationPendingIntent, error parsing", ex);
        }
        final Intent notificationIntent = buildNotificationIntent(message, builder);
        PendingIntent res = PendingIntent.getActivity(mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        return res;
    }

    private void showNotification(JSONObject message) {
        NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext);

        // These attributes are required
        final String id;
        try {
            builder.setContentTitle(message.getString(TITLE_ATTR));
            builder.setContentText(message.getString(TEXT_ATTR));
            id = message.getString(ID_ATTR);
        } catch (JSONException ex) {
            Log.i(LOGTAG, "Error parsing", ex);
            return;
        }

        Uri imageUri = Uri.parse(message.optString(SMALLICON_ATTR));
        builder.setSmallIcon(BitmapUtils.getResource(imageUri, R.drawable.ic_status_logo));

        JSONArray light = message.optJSONArray(LIGHT_ATTR);
        if (light != null && light.length() == 3) {
            try {
                builder.setLights(light.getInt(0),
                                  light.getInt(1),
                                  light.getInt(2));
            } catch (JSONException ex) {
                Log.i(LOGTAG, "Error parsing", ex);
            }
        }

        boolean ongoing = message.optBoolean(ONGOING_ATTR);
        builder.setOngoing(ongoing);

        if (message.has(WHEN_ATTR)) {
            long when = message.optLong(WHEN_ATTR);
            builder.setWhen(when);
        }

        if (message.has(PRIORITY_ATTR)) {
            int priority = message.optInt(PRIORITY_ATTR);
            builder.setPriority(priority);
        }

        if (message.has(LARGE_ICON_ATTR)) {
            Bitmap b = BitmapUtils.getBitmapFromDataURI(message.optString(LARGE_ICON_ATTR));
            builder.setLargeIcon(b);
        }

        if (message.has(PROGRESS_VALUE_ATTR) &&
            message.has(PROGRESS_MAX_ATTR) &&
            message.has(PROGRESS_INDETERMINATE_ATTR)) {
            try {
                final int progress = message.getInt(PROGRESS_VALUE_ATTR);
                final int progressMax = message.getInt(PROGRESS_MAX_ATTR);
                final boolean progressIndeterminate = message.getBoolean(PROGRESS_INDETERMINATE_ATTR);
                builder.setProgress(progressMax, progress, progressIndeterminate);
            } catch (JSONException ex) {
                Log.i(LOGTAG, "Error parsing", ex);
            }
        }

        JSONArray actions = message.optJSONArray(ACTIONS_ATTR);
        if (actions != null) {
            try {
                for (int i = 0; i < actions.length(); i++) {
                    JSONObject action = actions.getJSONObject(i);
                    final PendingIntent pending = buildButtonClickPendingIntent(message, action);
                    final String actionTitle = action.getString(ACTION_TITLE_ATTR);
                    final Uri actionImage = Uri.parse(action.optString(ACTION_ICON_ATTR));
                    builder.addAction(BitmapUtils.getResource(actionImage, R.drawable.ic_status_logo),
                                      actionTitle,
                                      pending);
                }
            } catch (JSONException ex) {
                Log.i(LOGTAG, "Error parsing", ex);
            }
        }

        PendingIntent pi = buildNotificationPendingIntent(message, CLICK_EVENT);
        builder.setContentIntent(pi);
        PendingIntent deletePendingIntent = buildNotificationPendingIntent(message, CLEARED_EVENT);
        builder.setDeleteIntent(deletePendingIntent);

        GeckoAppShell.notificationClient.add(id.hashCode(), builder.build());

        boolean persistent = message.optBoolean(PERSISTENT_ATTR);
        // We add only not persistent notifications to the list since we want to purge only
        // them when geckoapp is destroyed.
        if (!persistent && !mClearableNotifications.containsKey(id)) {
            mClearableNotifications.put(id, message.toString());
        }
    }

    private void hideNotification(JSONObject message) {
        final String id;
        final String handler;
        final String cookie;
        try {
            id = message.getString("id");
            handler = message.optString("handlerKey");
            cookie  = message.optString("cookie");
        } catch (JSONException ex) {
            Log.i(LOGTAG, "Error parsing", ex);
            return;
        }

        hideNotification(id, handler, cookie);
    }

    private void sendNotificationWasClosed(String id, String handlerKey, String cookie) {
        final JSONObject args = new JSONObject();
        try {
            args.put(ID_ATTR, id);
            args.put(HANDLER_ATTR, handlerKey);
            args.put(COOKIE_ATTR, cookie);
            args.put(EVENT_TYPE_ATTR, CLOSED_EVENT);
            Log.i(LOGTAG, "Send " + args.toString());
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Notification:Event", args.toString()));
        } catch (JSONException ex) {
            Log.e(LOGTAG, "sendNotificationWasClosed: error building JSON notification arguments.", ex);
        }
    }

    private void closeNotification(String id, String handlerKey, String cookie) {
        GeckoAppShell.notificationClient.remove(id.hashCode());
        sendNotificationWasClosed(id, handlerKey, cookie);
    }

    public void hideNotification(String id, String handlerKey, String cookie) {
        mClearableNotifications.remove(id);
        closeNotification(id, handlerKey, cookie);
    }

    private void clearAll() {
        for (Iterator<String> i = mClearableNotifications.keySet().iterator(); i.hasNext();) {
            final String id = i.next();
            final String json = mClearableNotifications.get(id);
            i.remove();

            JSONObject obj;
            try {
                obj = new JSONObject(json);
            } catch(JSONException ex) {
                obj = new JSONObject();
            }

            closeNotification(id, obj.optString(HANDLER_ATTR), obj.optString(COOKIE_ATTR));
        }
    }

    public static void destroy() {
        if (sInstance != null) {
            sInstance.clearAll();
        }
    }
}
