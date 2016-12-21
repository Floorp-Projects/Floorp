/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import java.io.File;
import java.io.UnsupportedEncodingException;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.util.GeckoEventListener;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

public final class NotificationHelper implements GeckoEventListener {
    public static final String HELPER_BROADCAST_ACTION = AppConstants.ANDROID_PACKAGE_NAME + ".helperBroadcastAction";

    public static final String NOTIFICATION_ID = "NotificationHelper_ID";
    private static final String LOGTAG = "GeckoNotificationHelper";
    private static final String HELPER_NOTIFICATION = "helperNotif";

    // Attributes mandatory to be used while sending a notification from js.
    private static final String TITLE_ATTR = "title";
    private static final String TEXT_ATTR = "text";
    /* package */ static final String ID_ATTR = "id";
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
    private static final String ACTIONS_ATTR = "actions";
    private static final String ACTION_ID_ATTR = "buttonId";
    private static final String ACTION_TITLE_ATTR = "title";
    private static final String ACTION_ICON_ATTR = "icon";
    private static final String PERSISTENT_ATTR = "persistent";
    private static final String HANDLER_ATTR = "handlerKey";
    private static final String COOKIE_ATTR = "cookie";
    static final String EVENT_TYPE_ATTR = "eventType";

    private static final String NOTIFICATION_SCHEME = "moz-notification";

    private static final String BUTTON_EVENT = "notification-button-clicked";
    private static final String CLICK_EVENT = "notification-clicked";
    static final String CLEARED_EVENT = "notification-cleared";

    static final String ORIGINAL_EXTRA_COMPONENT = "originalComponent";

    private final Context mContext;

    // Holds a list of notifications that should be cleared if the Fennec Activity is shut down.
    // Will not include ongoing or persistent notifications that are tied to Gecko's lifecycle.
    private HashMap<String, String> mClearableNotifications;

    private boolean mInitialized;
    private static NotificationHelper sInstance;

    private NotificationHelper(Context context) {
        mContext = context;
    }

    public void init() {
        if (mInitialized) {
            return;
        }

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

    public static void getArgsAndSendNotificationIntent(SafeIntent intent) {
        final JSONObject args = new JSONObject();
        final Uri data = intent.getData();

        final String notificationType = data.getQueryParameter(EVENT_TYPE_ATTR);

        try {
            args.put(ID_ATTR, data.getQueryParameter(ID_ATTR));
            args.put(EVENT_TYPE_ATTR, notificationType);
            args.put(HANDLER_ATTR, data.getQueryParameter(HANDLER_ATTR));
            args.put(COOKIE_ATTR, intent.getStringExtra(COOKIE_ATTR));

            if (BUTTON_EVENT.equals(notificationType)) {
                final String actionName = data.getQueryParameter(ACTION_ID_ATTR);
                args.put(ACTION_ID_ATTR, actionName);
            }

            Log.i(LOGTAG, "Send " + args.toString());
            GeckoAppShell.notifyObservers("Notification:Event", args.toString());
        } catch (JSONException e) {
            Log.e(LOGTAG, "Error building JSON notification arguments.", e);
        }
    }

    public void handleNotificationIntent(SafeIntent i) {
        final Uri data = i.getData();
        final String notificationType = data.getQueryParameter(EVENT_TYPE_ATTR);
        final String id = data.getQueryParameter(ID_ATTR);
        if (id == null || notificationType == null) {
            Log.e(LOGTAG, "handleNotificationEvent: invalid intent parameters");
            return;
        }

        getArgsAndSendNotificationIntent(i);

        // If the notification was clicked, we are closing it. This must be executed after
        // sending the event to js side because when the notification is canceled no event can be
        // handled.
        if (CLICK_EVENT.equals(notificationType) && !i.getBooleanExtra(ONGOING_ATTR, false)) {
            // The handler and cookie parameters are optional.
            final String handler = data.getQueryParameter(HANDLER_ATTR);
            final String cookie = i.getStringExtra(COOKIE_ATTR);
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

        // All intents get routed through the notificationReceiver. That lets us bail if we don't want to start Gecko
        final ComponentName name = new ComponentName(mContext, GeckoAppShell.getGeckoInterface().getActivity().getClass());
        notificationIntent.putExtra(ORIGINAL_EXTRA_COMPONENT, name);

        return notificationIntent;
    }

    private PendingIntent buildNotificationPendingIntent(JSONObject message, String type) {
        Uri.Builder builder = getNotificationBuilder(message, type);
        final Intent notificationIntent = buildNotificationIntent(message, builder);
        return PendingIntent.getBroadcast(mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntent buildButtonClickPendingIntent(JSONObject message, JSONObject action) {
        Uri.Builder builder = getNotificationBuilder(message, BUTTON_EVENT);
        try {
            // Action name must be in query uri, otherwise buttons pending intents
            // would be collapsed.
            if (action.has(ACTION_ID_ATTR)) {
                builder.appendQueryParameter(ACTION_ID_ATTR, action.getString(ACTION_ID_ATTR));
            } else {
                Log.i(LOGTAG, "button event with no name");
            }
        } catch (JSONException ex) {
            Log.i(LOGTAG, "buildNotificationPendingIntent, error parsing", ex);
        }
        final Intent notificationIntent = buildNotificationIntent(message, builder);
        PendingIntent res = PendingIntent.getBroadcast(mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
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
        builder.setSmallIcon(BitmapUtils.getResource(mContext, imageUri));

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
                    builder.addAction(BitmapUtils.getResource(mContext, actionImage),
                                      actionTitle,
                                      pending);
                }
            } catch (JSONException ex) {
                Log.i(LOGTAG, "Error parsing", ex);
            }
        }

        // Bug 1320889 - Tapping the notification for a downloaded file shouldn't open firefox.
        // If the gecko event is for download completion, we create another intent with different
        // scheme to prevent Fennec from popping up.
        final Intent viewFileIntent = createIntentIfDownloadCompleted(message);
        if (builder != null && viewFileIntent != null && mContext != null) {
            PendingIntent pIntent = PendingIntent.getActivity(mContext, 0, viewFileIntent, PendingIntent.FLAG_UPDATE_CURRENT);
            builder.setAutoCancel(true);
            builder.setContentIntent(pIntent);

        } else {
            PendingIntent pi = buildNotificationPendingIntent(message, CLICK_EVENT);
            builder.setContentIntent(pi);
            PendingIntent deletePendingIntent = buildNotificationPendingIntent(message, CLEARED_EVENT);
            builder.setDeleteIntent(deletePendingIntent);

        }

        ((NotificationClient) GeckoAppShell.getNotificationListener()).add(id, builder.build());

        boolean persistent = message.optBoolean(PERSISTENT_ATTR);
        // We add only not persistent notifications to the list since we want to purge only
        // them when geckoapp is destroyed.
        if (!persistent && !mClearableNotifications.containsKey(id)) {
            mClearableNotifications.put(id, message.toString());
        }
    }


    private Intent createIntentIfDownloadCompleted(JSONObject message) {
        try {
            if (message.has(HANDLER_ATTR) && message.get(HANDLER_ATTR).equals("downloads") &&
                    message.has(ONGOING_ATTR) && !message.optBoolean(ONGOING_ATTR) &&
                    message.has(COOKIE_ATTR) && message.getString(COOKIE_ATTR).split("http").length > 0) {

                String fileName = message.getString(TEXT_ATTR);
                String cookie = message.getString(COOKIE_ATTR);
                if (cookie.contains(fileName)) {
                    String filePath = cookie.substring(0, cookie.indexOf(fileName)).replace("\"", "");
                    String filePathDecode = java.net.URLDecoder.decode(filePath, "UTF-8");
                    Uri uri = Uri.fromFile(new File(filePathDecode + fileName));
                    Intent intent = new Intent();
                    intent.setAction(Intent.ACTION_VIEW);
                    intent.setDataAndType(uri, URLConnection.guessContentTypeFromName(uri.toString()));

                    // if no one can handle this intent, let the user decides
                    PackageManager manager = mContext.getPackageManager();
                    List<ResolveInfo> infos = manager.queryIntentActivities(intent, 0);
                    if (infos.size() == 0) {
                        intent.setDataAndType(uri, "*/*");
                    }

                    return intent;
                }
            }
        } catch (JSONException je) {
            Log.e(LOGTAG, "Error while parsing download complete event.", je);
            return null;
        } catch (UnsupportedEncodingException e) {
            Log.e(LOGTAG, "Error while parsing download file path.", e);
            return null;
        }
        return null;
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

    private void closeNotification(String id, String handlerKey, String cookie) {
        ((NotificationClient) GeckoAppShell.getNotificationListener()).remove(id);
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
            } catch (JSONException ex) {
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
