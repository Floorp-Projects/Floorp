/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.PixelFormat;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.provider.Settings;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.text.TextUtils;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.preferences.GeckoPreferences;

import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;


/**
 * On launch this Service displays a View over the currently running process with an action to open the url in Fennec
 * immediately.  If the user takes no action, allowing the runnable to be processed after the specified
 * timeout (TOAST_TIMEOUT), the url is added to a file which is then read in Fennec on next launch, this allows the
 * user to quickly queue urls to open without having to open Fennec each time. If the Service receives an Intent whilst
 * the created View is still active, the old url is immediately processed and the View is re-purposed with the new
 * Intent data.
 * <p/>
 * The SYSTEM_ALERT_WINDOW permission is used to allow us to insert a View from this Service which responds to user
 * interaction, whilst still allowing whatever is in the background to be seen and interacted with.
 * <p/>
 * Using an Activity to do this doesn't seem to work as there's an issue to do with the native android intent resolver
 * dialog not being hidden when the toast is shown.  Using an IntentService instead of a Service doesn't work as
 * each new Intent received kicks off the IntentService lifecycle anew which means that a new View is created each time,
 * meaning that we can't quickly queue the current data and re-purpose the View.  The asynchronous nature of the
 * IntentService is another prohibitive factor.
 * <p/>
 * General approach taken is similar to the FB chat heads functionality:
 * http://stackoverflow.com/questions/15975988/what-apis-in-android-is-facebook-using-to-create-chat-heads
 */
public class TabQueueService extends Service {
    private static final String LOGTAG = "Gecko" + TabQueueService.class.getSimpleName();

    private static final long TOAST_TIMEOUT = 3000;
    private static final long TOAST_DOUBLE_TAP_TIMEOUT_MILLIS = 6000;

    private WindowManager windowManager;
    private View toastLayout;
    private Button openNowButton;
    private Handler tabQueueHandler;
    private WindowManager.LayoutParams toastLayoutParams;
    private volatile StopServiceRunnable stopServiceRunnable;
    private HandlerThread handlerThread;
    private ExecutorService executorService;

    @Override
    public IBinder onBind(Intent intent) {
        // Not used
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        executorService = Executors.newSingleThreadExecutor();

        handlerThread = new HandlerThread("TabQueueHandlerThread");
        handlerThread.start();
        tabQueueHandler = new Handler(handlerThread.getLooper());

        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        LayoutInflater layoutInflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
        toastLayout = layoutInflater.inflate(R.layout.tab_queue_toast, null);

        final Resources resources = getResources();

        TextView messageView = (TextView) toastLayout.findViewById(R.id.toast_message);
        messageView.setText(resources.getText(R.string.tab_queue_toast_message));

        openNowButton = (Button) toastLayout.findViewById(R.id.toast_button);
        openNowButton.setText(resources.getText(R.string.tab_queue_toast_action));

        toastLayoutParams = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                AppConstants.Versions.preO ?
                        WindowManager.LayoutParams.TYPE_PHONE :
                        WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL |
                        WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH |
                        WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);

        toastLayoutParams.gravity = Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
    }

    @Override
    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        // If this is a redelivery then lets bypass the entire double tap to open now code as that's a big can of worms,
        // we also don't expect redeliveries because of the short time window associated with this feature.
        if (flags != START_FLAG_REDELIVERY) {
            final Context applicationContext = getApplicationContext();
            final SharedPreferences sharedPreferences = GeckoSharedPrefs.forApp(applicationContext);

            final String lastUrl = sharedPreferences.getString(GeckoPreferences.PREFS_TAB_QUEUE_LAST_SITE, "");

            final SafeIntent safeIntent = new SafeIntent(intent);
            final String intentUrl = safeIntent.getDataString();

            final long lastRunTime = sharedPreferences.getLong(GeckoPreferences.PREFS_TAB_QUEUE_LAST_TIME, 0);
            final boolean isWithinDoubleTapTimeLimit = System.currentTimeMillis() - lastRunTime < TOAST_DOUBLE_TAP_TIMEOUT_MILLIS;

            if (!TextUtils.isEmpty(lastUrl) && lastUrl.equals(intentUrl) && isWithinDoubleTapTimeLimit) {
                // Background thread because we could do some file IO if we have to remove a url from the list.
                tabQueueHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        // If there is a runnable around, that means that the previous process hasn't yet completed, so
                        // we will need to prevent it from running and remove the view from the window manager.
                        // If there is no runnable around then the url has already been added to the list, so we'll
                        // need to remove it before proceeding or that url will open multiple times.
                        if (stopServiceRunnable != null) {
                            tabQueueHandler.removeCallbacks(stopServiceRunnable);
                            stopSelfResult(stopServiceRunnable.getStartId());
                            stopServiceRunnable = null;
                            removeView();
                        } else {
                            TabQueueHelper.removeURLFromFile(applicationContext, intentUrl, TabQueueHelper.FILE_NAME);
                        }
                        openNow(safeIntent.getUnsafe());

                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "tabqueue-doubletap");
                        stopSelfResult(startId);
                    }
                });

                return START_REDELIVER_INTENT;
            }

            sharedPreferences.edit().putString(GeckoPreferences.PREFS_TAB_QUEUE_LAST_SITE, intentUrl)
                                    .putLong(GeckoPreferences.PREFS_TAB_QUEUE_LAST_TIME, System.currentTimeMillis())
                                    .apply();
        }

        if (stopServiceRunnable != null) {
            // If we're already displaying a toast, keep displaying it but store the previous url.
            // The open button will refer to the most recently opened link.
            tabQueueHandler.removeCallbacks(stopServiceRunnable);
            stopServiceRunnable.run(false);
        } else {
            try {
                windowManager.addView(toastLayout, toastLayoutParams);
            } catch (final SecurityException | WindowManager.BadTokenException e) {
                Toast.makeText(this, getText(R.string.tab_queue_toast_message), Toast.LENGTH_SHORT).show();
                showSettingsNotification();
            }
        }

        stopServiceRunnable = new StopServiceRunnable(startId) {
            @Override
            public void onRun() {
                addURLToTabQueue(intent, TabQueueHelper.FILE_NAME);
                stopServiceRunnable = null;
            }
        };

        openNowButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(final View view) {
                tabQueueHandler.removeCallbacks(stopServiceRunnable);
                stopServiceRunnable = null;
                removeView();
                openNow(intent);

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "tabqueue-now");
                stopSelfResult(startId);
            }
        });

        tabQueueHandler.postDelayed(stopServiceRunnable, TOAST_TIMEOUT);

        return START_REDELIVER_INTENT;
    }

    private void openNow(Intent intent) {
        Intent forwardIntent = new Intent(intent);
        forwardIntent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        forwardIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(forwardIntent);

        TabQueueHelper.removeNotification(getApplicationContext());

        GeckoSharedPrefs.forApp(getApplicationContext()).edit().remove(GeckoPreferences.PREFS_TAB_QUEUE_LAST_SITE)
                                                               .remove(GeckoPreferences.PREFS_TAB_QUEUE_LAST_TIME)
                                                               .apply();

        executorService.submit(new Runnable() {
            @Override
            public void run() {
                int queuedTabCount = TabQueueHelper.getTabQueueLength(TabQueueService.this);
                Telemetry.addToHistogram("FENNEC_TABQUEUE_QUEUESIZE", queuedTabCount);
            }
        });

    }

    @SuppressLint("NewApi")
    @TargetApi(Build.VERSION_CODES.M)
    private void showSettingsNotification() {
        if (AppConstants.Versions.preMarshmallow) {
            return;
        }

        final Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
        intent.setData(Uri.parse("package:" + getPackageName()));
        PendingIntent pendingIntent = PendingIntent.getActivity(this, intent.hashCode(), intent, 0);

        final String text = getString(R.string.tab_queue_notification_settings);

        final NotificationCompat.BigTextStyle style = new NotificationCompat.BigTextStyle()
                .bigText(text);

        final NotificationCompat.Builder notificationBuilder = new NotificationCompat.Builder(this)
                .setContentTitle(getString(R.string.pref_tab_queue_title))
                .setContentText(text)
                .setCategory(NotificationCompat.CATEGORY_ERROR)
                .setStyle(style)
                .setSmallIcon(R.drawable.ic_status_logo)
                .setContentIntent(pendingIntent)
                .setPriority(NotificationCompat.PRIORITY_MAX)
                .setAutoCancel(true)
                .addAction(R.drawable.ic_action_settings, getString(R.string.tab_queue_prompt_settings_button), pendingIntent);

        if (!AppConstants.Versions.preO) {
            notificationBuilder.setChannelId(GeckoApplication.getDefaultNotificationChannel().getId());
        }

        NotificationManager notificationManager = (NotificationManager) this.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.notify(R.id.tabQueueSettingsNotification, notificationBuilder.build());
    }

    private void removeView() {
        try {
            windowManager.removeView(toastLayout);
        } catch (IllegalArgumentException | IllegalStateException e) {
            // This can happen if the Service is killed by the system.  If this happens the View will have already
            // been removed but the runnable will have been kept alive.
            Log.e(LOGTAG, "Error removing Tab Queue toast from service", e);
        }
    }

    private void addURLToTabQueue(final Intent intent, final String filename) {
        if (intent == null) {
            // This should never happen, but let's return silently instead of crashing if it does.
            Log.w(LOGTAG, "Error adding URL to tab queue - invalid intent passed in.");
            return;
        }
        final SafeIntent safeIntent = new SafeIntent(intent);
        final String intentData = safeIntent.getDataString();

        // As we're doing disk IO, let's run this stuff in a separate thread.
        executorService.submit(new Runnable() {
            @Override
            public void run() {
                Context applicationContext = getApplicationContext();
                final GeckoProfile profile = GeckoProfile.get(applicationContext);
                int tabsQueued = TabQueueHelper.queueURL(profile, intentData, filename);
                List<String> urls = TabQueueHelper.getLastURLs(applicationContext, filename);

                TabQueueHelper.showNotification(applicationContext, tabsQueued, urls);

                // Store the number of URLs queued so that we don't have to read and process the file to see if we have
                // any urls to open.
                // TODO: Use profile shared prefs when bug 1147925 gets fixed.
                final SharedPreferences prefs = GeckoSharedPrefs.forApp(applicationContext);

                prefs.edit().putInt(TabQueueHelper.PREF_TAB_QUEUE_COUNT, tabsQueued).apply();
            }
        });
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        handlerThread.quit();
    }

    /**
     * A modified Runnable which additionally removes the view from the window view hierarchy and stops the service
     * when run, unless explicitly instructed not to.
     */
    private abstract class StopServiceRunnable implements Runnable {

        private final int startId;

        public StopServiceRunnable(final int startId) {
            this.startId = startId;
        }

        public void run() {
            run(true);
        }

        public void run(final boolean shouldRemoveView) {
            onRun();

            if (shouldRemoveView) {
                removeView();
            }

            stopSelfResult(startId);
        }

        public int getStartId() {
            return startId;
        }

        public abstract void onRun();
    }
}