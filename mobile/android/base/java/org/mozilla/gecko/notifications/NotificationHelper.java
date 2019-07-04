/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.notifications;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.net.Uri;
import android.support.v4.app.NotificationCompat;
import android.support.v4.util.SimpleArrayMap;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoActivityMonitor;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.BitmapUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.StrictModeContext;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.io.UnsupportedEncodingException;
import java.net.URLConnection;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class NotificationHelper implements BundleEventListener {
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


    public enum Channel {
        /**
         * Default notification channel.
         */
        DEFAULT,
        /**
         * Mozilla Location Services notification channel.
         */
        MLS,
        /**
         * File downloads.
         */
        DOWNLOAD,
        /**
         *  Media notification channel
         */
        MEDIA,
        /**
         * Built-in updater - use only when <code>AppConstants.MOZ_UPDATER</code> is true.
         */
        UPDATER,
        /**
         * Synced tabs notification channel
         */
        SYNCED_TABS,
        /**
         * Leanplum notification channel - use only when <code>AppConstants.MOZ_ANDROID_MMA</code> is true.
         */
        LP_DEFAULT,
        /**
         * HTML5 web site notifications
         */
        SITE_NOTIFICATIONS,
        /**
         * Crash handler notification used on Android Q+
         */
        CRASH_HANDLER,
    }

    // Holds the mapping between the Channel enum used by the rest of our codebase and the
    // channel ID used for communication with the system NotificationManager.
    // How to determine the initialCapacity: Count all channels (including the Updater, which is
    // only added further down in initNotificationChannels), multiply by 4/3 for a maximum load
    // factor of 75 % and round up to the next multiple of two.
    private final Map<Channel, String> mDefinedNotificationChannels = new HashMap<Channel, String>(16) {{
        final String DEFAULT_CHANNEL_TAG = "default2-notification-channel";
        put(Channel.DEFAULT, DEFAULT_CHANNEL_TAG);

        final String MLS_CHANNEL_TAG = "mls-notification-channel";
        put(Channel.MLS, MLS_CHANNEL_TAG);

        final String DOWNLOAD_NOTIFICATION_TAG = "download-notification-channel";
        put(Channel.DOWNLOAD, DOWNLOAD_NOTIFICATION_TAG);

        final String MEDIA_CHANNEL_TAG = "media-notification-channel";
        put(Channel.MEDIA, MEDIA_CHANNEL_TAG);

        if (AppConstants.MOZ_ANDROID_MMA) {
            final String LP_DEFAULT_CHANNEL_TAG = "lp-default-notification-channel";
            put(Channel.LP_DEFAULT, LP_DEFAULT_CHANNEL_TAG);
        }

        final String SYNCED_TABS_CHANNEL_TAG = "synced-tabs-notification-channel";
        put(Channel.SYNCED_TABS, SYNCED_TABS_CHANNEL_TAG);

        final String SITE_NOTIFICATIONS_CHANNEL_TAG = "site-notifications";
        put(Channel.SITE_NOTIFICATIONS, SITE_NOTIFICATIONS_CHANNEL_TAG);

        final String CRASH_HANDLER_CHANNEL_TAG = "crash-handler";
        put(Channel.CRASH_HANDLER, CRASH_HANDLER_CHANNEL_TAG);
    }};

    // These are channels we no longer require and want to retire from Android's settings UI.
    private final List<String> mDeprecatedNotificationChannels = new ArrayList<>(Arrays.asList(
            "default-notification-channel",
            null
    ));

    // Holds a list of notifications that should be cleared if the Fennec Activity is shut down.
    // Will not include ongoing or persistent notifications that are tied to Gecko's lifecycle.
    private SimpleArrayMap<String, GeckoBundle> mClearableNotifications;

    private boolean mInitialized;
    private static NotificationHelper sInstance;

    private NotificationHelper(Context context) {
        mContext = context;
    }

    public void init() {
        if (mInitialized) {
            return;
        }

        mClearableNotifications = new SimpleArrayMap<>();
        EventDispatcher.getInstance().registerUiThreadListener(this,
            "Notification:Show",
            "Notification:Hide");

        if (!AppConstants.Versions.preO) {
            initNotificationChannels();
        }

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

    private void initNotificationChannels() {
        final String UPDATER_CHANNEL_TAG = "updater-notification-channel";
        if (UpdateServiceHelper.isUpdaterEnabled(mContext)) {
            mDefinedNotificationChannels.put(Channel.UPDATER, UPDATER_CHANNEL_TAG);
        } else {
            mDeprecatedNotificationChannels.add(UPDATER_CHANNEL_TAG);
        }

        final NotificationManager notificationManager =
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);

        for (String channelId : mDeprecatedNotificationChannels) {
            removeChannel(notificationManager, channelId);
        }

        for (Channel mozChannel : mDefinedNotificationChannels.keySet()) {
            createChannel(notificationManager, mozChannel);
        }
    }

    @SuppressWarnings("fallthrough")
    @TargetApi(26)
    private void createChannel(final NotificationManager manager, Channel definedChannel) {
        NotificationChannel channel =
                manager.getNotificationChannel(mDefinedNotificationChannels.get(definedChannel));

        if (channel == null) {
            switch (definedChannel) {
                case MLS: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.mls_notification_channel),
                            NotificationManager.IMPORTANCE_LOW);
                }
                break;

                case DOWNLOAD: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.download_notification_channel),
                            NotificationManager.IMPORTANCE_LOW);
                }
                break;

                case MEDIA: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.media_notification_channel2),
                            NotificationManager.IMPORTANCE_LOW);
                }
                break;

                case UPDATER: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.updater_notification_channel),
                            NotificationManager.IMPORTANCE_LOW);
                }
                break;

                case SYNCED_TABS: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.synced_tabs_notification_channel),
                            NotificationManager.IMPORTANCE_HIGH);
                }
                break;

                case LP_DEFAULT: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.leanplum_default_notifications_channel2),
                            NotificationManager.IMPORTANCE_LOW);
                }
                break;

                case SITE_NOTIFICATIONS: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.site_notifications_channel),
                            NotificationManager.IMPORTANCE_DEFAULT);
                }
                break;

                case CRASH_HANDLER: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.crash_handler_notifications_channel),
                            AppConstants.Versions.feature29Plus ?
                                    NotificationManager.IMPORTANCE_HIGH :
                                    NotificationManager.IMPORTANCE_LOW);
                }
                break;

                case DEFAULT:
                default: {
                    channel = new NotificationChannel(mDefinedNotificationChannels.get(definedChannel),
                            mContext.getString(R.string.default_notification_channel2),
                            NotificationManager.IMPORTANCE_LOW);
                }
                break;
            }

            manager.createNotificationChannel(channel);
        }
    }

    @TargetApi(26)
    private void removeChannel(final NotificationManager manager, final String channelId) {
        if (manager.getNotificationChannel(channelId) != null) {
            manager.deleteNotificationChannel(channelId);
        }
    }

    @TargetApi(26)
    public NotificationChannel getNotificationChannel(Channel definedChannel) {
        final NotificationManager notificationManager = (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE);
        return notificationManager.getNotificationChannel(mDefinedNotificationChannels.get(definedChannel));
    }

    @Override // BundleEventListener
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if ("Notification:Show".equals(event)) {
            showNotification(message);
        } else if ("Notification:Hide".equals(event)) {
            hideNotification(message);
        }
    }

    public boolean isHelperIntent(Intent i) {
        return i.getBooleanExtra(HELPER_NOTIFICATION, false);
    }

    public static void getArgsAndSendNotificationIntent(SafeIntent intent) {
        final GeckoBundle args = new GeckoBundle(5);
        final Uri data = intent.getData();

        final String notificationType = data.getQueryParameter(EVENT_TYPE_ATTR);

        args.putString(ID_ATTR, data.getQueryParameter(ID_ATTR));
        args.putString(EVENT_TYPE_ATTR, notificationType);
        args.putString(HANDLER_ATTR, data.getQueryParameter(HANDLER_ATTR));
        args.putString(COOKIE_ATTR, intent.getStringExtra(COOKIE_ATTR));

        if (BUTTON_EVENT.equals(notificationType)) {
            final String actionName = data.getQueryParameter(ACTION_ID_ATTR);
            args.putString(ACTION_ID_ATTR, actionName);
        }

        EventDispatcher.getInstance().dispatch("Notification:Event", args);
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

    private Uri.Builder getNotificationBuilder(final GeckoBundle message, final String type) {
        final Uri.Builder b = new Uri.Builder();
        b.scheme(NOTIFICATION_SCHEME).appendQueryParameter(EVENT_TYPE_ATTR, type);

        final String id = message.getString(ID_ATTR);
        b.appendQueryParameter(ID_ATTR, id);

        final String handler = message.getString(HANDLER_ATTR);
        b.appendQueryParameter(HANDLER_ATTR, handler);

        return b;
    }

    private Intent buildNotificationIntent(final GeckoBundle message, final Uri.Builder builder) {
        final Intent notificationIntent = new Intent(HELPER_BROADCAST_ACTION);
        notificationIntent.setClass(mContext, NotificationReceiver.class);
        final boolean ongoing = message.getBoolean(ONGOING_ATTR);
        notificationIntent.putExtra(ONGOING_ATTR, ongoing);

        final Uri dataUri = builder.build();
        notificationIntent.setData(dataUri);
        notificationIntent.putExtra(HELPER_NOTIFICATION, true);
        notificationIntent.putExtra(COOKIE_ATTR, message.getString(COOKIE_ATTR, ""));

        // All intents get routed through the notificationReceiver. That lets us bail if we don't want to start Gecko
        final Activity currentActivity =
                GeckoActivityMonitor.getInstance().getCurrentActivity();
        final ComponentName name;
        if (currentActivity != null) {
            name = new ComponentName(mContext, currentActivity.getClass());
        } else {
            name = new ComponentName(mContext, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        }
        notificationIntent.putExtra(ORIGINAL_EXTRA_COMPONENT, name);

        return notificationIntent;
    }

    private PendingIntent buildNotificationPendingIntent(final GeckoBundle message,
                                                         final String type) {
        final Uri.Builder builder = getNotificationBuilder(message, type);
        final Intent notificationIntent = buildNotificationIntent(message, builder);
        return PendingIntent.getBroadcast(
                mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private PendingIntent buildButtonClickPendingIntent(final GeckoBundle message,
                                                        final GeckoBundle action) {
        Uri.Builder builder = getNotificationBuilder(message, BUTTON_EVENT);

        // Action name must be in query uri, otherwise buttons pending intents
        // would be collapsed.
        if (action.containsKey(ACTION_ID_ATTR)) {
            builder.appendQueryParameter(ACTION_ID_ATTR, action.getString(ACTION_ID_ATTR));
        } else {
            Log.i(LOGTAG, "button event with no name");
        }

        final Intent notificationIntent = buildNotificationIntent(message, builder);
        PendingIntent res = PendingIntent.getBroadcast(
                mContext, 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);
        return res;
    }

    @SuppressWarnings({"NewApi", "try"})
    private void showNotification(final GeckoBundle message) {
        ThreadUtils.assertOnUiThread();

        final NotificationCompat.Builder builder = new NotificationCompat.Builder(mContext);

        // These attributes are required
        final String id = message.getString(ID_ATTR);
        builder.setContentTitle(message.getString(TITLE_ATTR));
        builder.setContentText(message.getString(TEXT_ATTR));

        final Uri imageUri = Uri.parse(message.getString(SMALLICON_ATTR, ""));
        builder.setSmallIcon(BitmapUtils.getResource(mContext, imageUri));

        final int[] light = message.getIntArray(LIGHT_ATTR);
        if (light != null && light.length == 3) {
            builder.setLights(light[0], light[1], light[2]);
        }

        final boolean ongoing = message.getBoolean(ONGOING_ATTR);
        builder.setOngoing(ongoing);

        if (message.containsKey(WHEN_ATTR)) {
            final long when = (long) message.getDouble(WHEN_ATTR);
            builder.setWhen(when);
        }

        if (message.containsKey(PRIORITY_ATTR)) {
            final int priority = message.getInt(PRIORITY_ATTR);
            builder.setPriority(priority);
        }

        if (message.containsKey(LARGE_ICON_ATTR)) {
            final Bitmap b = BitmapUtils.getBitmapFromDataURI(
                    message.getString(LARGE_ICON_ATTR, ""));
            builder.setLargeIcon(b);
        }

        if (!AppConstants.Versions.preO) {
            if ("downloads".equals(message.getString(HANDLER_ATTR))) {
                builder.setChannelId(getNotificationChannel(Channel.DOWNLOAD).getId());
                builder.setOnlyAlertOnce(true);
            } else {
                builder.setChannelId(getNotificationChannel(Channel.DEFAULT).getId());
            }
        }

        if (message.containsKey(PROGRESS_VALUE_ATTR) &&
            message.containsKey(PROGRESS_MAX_ATTR) &&
            message.containsKey(PROGRESS_INDETERMINATE_ATTR)) {
            final int progress = message.getInt(PROGRESS_VALUE_ATTR);
            final int progressMax = message.getInt(PROGRESS_MAX_ATTR);
            final boolean progressIndeterminate = message.getBoolean(PROGRESS_INDETERMINATE_ATTR);
            builder.setProgress(progressMax, progress, progressIndeterminate);
        }

        final GeckoBundle[] actions = message.getBundleArray(ACTIONS_ATTR);
        if (actions != null) {
            for (int i = 0; i < actions.length; i++) {
                final GeckoBundle action = actions[i];
                final PendingIntent pending = buildButtonClickPendingIntent(message, action);
                final String actionTitle = action.getString(ACTION_TITLE_ATTR);
                final Uri actionImage = Uri.parse(action.getString(ACTION_ICON_ATTR, ""));
                builder.addAction(BitmapUtils.getResource(mContext, actionImage),
                                  actionTitle,
                                  pending);
            }
        }

        // Bug 1320889 - Tapping the notification for a downloaded file shouldn't open firefox.
        // If the gecko event is for download completion, we create another intent with different
        // scheme to prevent Fennec from popping up.
        final Intent viewFileIntent = createIntentIfDownloadCompleted(message);
        if (builder != null && viewFileIntent != null && mContext != null) {
            // Bug 1450449 - Downloaded files already are already in a public directory and aren't
            // really owned exclusively by Firefox, so there's no real benefit to using
            // content:// URIs here.
            try (StrictModeContext unused = StrictModeContext.allowAllVmPolicies()) {
                final PendingIntent pIntent = PendingIntent.getActivity(
                        mContext, 0, viewFileIntent, PendingIntent.FLAG_UPDATE_CURRENT);
                builder.setAutoCancel(true);
                builder.setContentIntent(pIntent);
            }

        } else {
            final PendingIntent pi = buildNotificationPendingIntent(message, CLICK_EVENT);
            final PendingIntent deletePendingIntent = buildNotificationPendingIntent(
                    message, CLEARED_EVENT);
            builder.setContentIntent(pi);
            builder.setDeleteIntent(deletePendingIntent);

        }

        ((NotificationClient) GeckoAppShell.getNotificationListener()).add(id, builder.build());

        final boolean persistent = message.getBoolean(PERSISTENT_ATTR);
        // We add only not persistent notifications to the list since we want to purge only
        // them when geckoapp is destroyed.
        if (!persistent && !mClearableNotifications.containsKey(id)) {
            mClearableNotifications.put(id, message);
        } else if (persistent && mClearableNotifications.containsKey(id)) {
            mClearableNotifications.remove(id);
        }
    }

    private Intent createIntentIfDownloadCompleted(final GeckoBundle message) {
        if (!"downloads".equals(message.get(HANDLER_ATTR)) ||
                message.getBoolean(ONGOING_ATTR, true)) {
            return null;
        }

        final String fileName = message.getString(TEXT_ATTR);
        final String cookie = message.getString(COOKIE_ATTR);
        if (!cookie.contains(fileName)) {
            return null;
        }

        final String filePath = cookie.substring(0, cookie.indexOf(fileName)).replace("\"", "");
        final String filePathDecode;
        try {
            filePathDecode = URLDecoder.decode(filePath, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            Log.e(LOGTAG, "Error while parsing download file path", e);
            return null;
        }

        final Uri uri = Uri.fromFile(new File(filePathDecode + fileName));
        final Intent intent = new Intent();
        intent.setAction(Intent.ACTION_VIEW);
        intent.setDataAndType(uri, URLConnection.guessContentTypeFromName(uri.toString()));

        // if no one can handle this intent, let the user decide.
        final PackageManager manager = mContext.getPackageManager();
        final List<ResolveInfo> infos = manager.queryIntentActivities(intent, 0);
        if (infos.size() == 0) {
            intent.setDataAndType(uri, "*/*");
        }

        return intent;
    }

    private void hideNotification(final GeckoBundle message) {
        final String id = message.getString("id");
        final String handler = message.getString("handlerKey", "");
        final String cookie = message.getString("cookie", "");

        hideNotification(id, handler, cookie);
    }

    private void closeNotification(String id, String handlerKey, String cookie) {
        ((NotificationClient) GeckoAppShell.getNotificationListener()).remove(id);
    }

    public void hideNotification(String id, String handlerKey, String cookie) {
        ThreadUtils.assertOnUiThread();

        mClearableNotifications.remove(id);
        closeNotification(id, handlerKey, cookie);
    }

    private void clearAll() {
        ThreadUtils.assertOnUiThread();

        for (int i = 0; i < mClearableNotifications.size(); i++) {
            final String id = mClearableNotifications.keyAt(i);
            final GeckoBundle obj = mClearableNotifications.valueAt(i);
            closeNotification(id, obj.getString(HANDLER_ATTR, ""), obj.getString(COOKIE_ATTR, ""));
        }
        mClearableNotifications.clear();
    }

    public static void destroy() {
        if (sInstance != null) {
            sInstance.clearAll();
        }
    }
}
