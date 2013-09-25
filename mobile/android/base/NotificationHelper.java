/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.GeckoEventListener;

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

import java.util.Set;
import java.util.HashSet;

public final class NotificationHelper implements GeckoEventListener {
    public static final String NOTIFICATION_ID = "NotificationHelper_ID";
    private static final String LOGTAG = "GeckoNotificationManager";
    private static final String HELPER_NOTIFICATION = "helperNotif";
    private static final String HELPER_BROADCAST_ACTION = "helperBroadcastAction";

    // Attributes mandatory to be used while sending a notification from js.
    private static final String TITLE_ATTR = "title";
    private static final String TEXT_ATTR = "text";
    private static final String ID_ATTR = "id";
    private static final String SMALLICON_ATTR = "smallIcon";

    // Attributes that can be used while sending a notification from js.
    private static final String PROGRESS_VALUE_ATTR = "progress_value";
    private static final String PROGRESS_MAX_ATTR = "progress_max";
    private static final String LIGHT_ATTR = "light";
    private static final String ONGOING_ATTR = "ongoing";
    private static final String WHEN_ATTR = "when";
    private static final String LARGE_ICON_ATTR = "largeIcon";
    private static final String COOKIE_ATTR = "cookie";
    private static final String EVENT_TYPE_ATTR = "eventType";
    private static final String ACTIONS_ATTR = "actions";
    private static final String ACTION_ATTR = "actionKind";
    private static final String ACTION_TITLE_ATTR = "title";
    private static final String ACTION_ICON_ATTR = "icon";

    private static final String NOTIFICATION_SCHEME = "moz-notification";

    private static final String CLICK_EVENT = "notification-clicked";
    private static final String CLEARED_EVENT = "notification-cleared";

    private static Context mContext;
    private static Set<String> mShowing;
    private static BroadcastReceiver mReceiver;
    private static NotificationHelper mInstance;

    private NotificationHelper() {
    }

    public static void init(Context context) {
        if (mInstance != null) {
            Log.w(LOGTAG, "NotificationHelper.init() called twice!");
        }
        mInstance = new NotificationHelper();
        mContext = context;
        mShowing = new HashSet<String>();
        registerEventListener("Notification:Show");
        registerEventListener("Notification:Hide");
        registerReceiver(context);
    }

    private static void registerEventListener(String event) {
        GeckoAppShell.getEventDispatcher().registerEventListener(event, mInstance);
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

    private static void registerReceiver(Context context) {
        IntentFilter filter = new IntentFilter(HELPER_BROADCAST_ACTION);
        // Scheme is needed, otherwise only broadcast with no data will be catched.
        filter.addDataScheme(NOTIFICATION_SCHEME);
        mReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mInstance.handleNotificationIntent(intent);
            }
        };
        context.registerReceiver(mReceiver, filter);
    }


    private void handleNotificationIntent(Intent i) {
        final Uri data = i.getData();
        if (data == null) {
            Log.w(LOGTAG, "handleNotificationEvent: empty data");
            return;
        }
        final String id = data.getQueryParameter(ID_ATTR);
        final String notificationType = data.getQueryParameter(EVENT_TYPE_ATTR);
        if (id == null || notificationType == null) {
            Log.w(LOGTAG, "handleNotificationEvent: invalid intent parameters");
            return;
        }

        // In case the user swiped out the notification, we empty the id
        // set.
        if (CLEARED_EVENT.equals(notificationType)) {
            mShowing.remove(id);
        }

        // If the notification was clicked, we are closing it.
        if (CLICK_EVENT.equals(notificationType) && !i.getBooleanExtra(ONGOING_ATTR, false)) {
            hideNotification(id);
        }
        String cookie = data.getQueryParameter(COOKIE_ATTR);

        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            JSONObject args = new JSONObject();
            try {
                args.put(NOTIFICATION_ID, id);
                if (cookie != null) {
                    args.put(COOKIE_ATTR, cookie);
                }
                args.put(EVENT_TYPE_ATTR, notificationType);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Notification:Event", args.toString()));
            } catch (JSONException e) {
                Log.w(LOGTAG, "Error building JSON notification arguments.", e);
            }
        }
    }

    private PendingIntent buildNotificationPendingIntent(JSONObject message, String type) {
        Intent notificationIntent = new Intent(HELPER_BROADCAST_ACTION);
        final boolean ongoing = message.optBoolean(ONGOING_ATTR);
        notificationIntent.putExtra(ONGOING_ATTR, ongoing);

        Uri.Builder b = new Uri.Builder();
        b.scheme(NOTIFICATION_SCHEME).appendQueryParameter(EVENT_TYPE_ATTR, type);

        try {
            final String id = message.getString(ID_ATTR);
            b.appendQueryParameter(ID_ATTR, id);
            if (message.has(COOKIE_ATTR)) {
                b.appendQueryParameter(COOKIE_ATTR,
                                       message.getString(COOKIE_ATTR));
            }
        } catch (JSONException ex) {
            Log.i(LOGTAG, "buildNotificationPendingIntent, error parsing", ex);
        }
        final Uri dataUri = b.build();
        notificationIntent.setData(dataUri);
        notificationIntent.putExtra(HELPER_NOTIFICATION, true);
        PendingIntent pi = PendingIntent.getBroadcast(mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        return pi;
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
            int when = message.optInt(WHEN_ATTR);
            builder.setWhen(when);
        }

        if (message.has(LARGE_ICON_ATTR)) {
            Bitmap b = BitmapUtils.getBitmapFromDataURI(message.optString(LARGE_ICON_ATTR));
            builder.setLargeIcon(b);
        }

        if (message.has(PROGRESS_VALUE_ATTR) &&
            message.has(PROGRESS_MAX_ATTR)) {
            try {
                final int progress = message.getInt(PROGRESS_VALUE_ATTR);
                final int progressMax = message.getInt(PROGRESS_MAX_ATTR);
                builder.setProgress(progressMax, progress, false);
            } catch (JSONException ex) {
                Log.i(LOGTAG, "Error parsing", ex);
            }
        }

        JSONArray actions = message.optJSONArray(ACTIONS_ATTR);
        if (actions != null) {
            try {
                for (int i = 0; i < actions.length(); i++) {
                    JSONObject action = actions.getJSONObject(i);
                    final PendingIntent pending = buildNotificationPendingIntent(message, action.getString(ACTION_ATTR));
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

        GeckoAppShell.sNotificationClient.add(id.hashCode(), builder.build());
        if (!mShowing.contains(id)) {
            mShowing.add(id);
        }
    }

    private void hideNotification(JSONObject message) {
        String id;
        try {
            id = message.getString("id");
        } catch (JSONException ex) {
            Log.i(LOGTAG, "Error parsing", ex);
            return;
        }

        hideNotification(id);
    }

    public void hideNotification(String id) {
        GeckoAppShell.sNotificationClient.remove(id.hashCode());
        mShowing.remove(id);
    }

    private void clearAll() {
        for (String id : mShowing) {
            hideNotification(id);
        }
    }
}
