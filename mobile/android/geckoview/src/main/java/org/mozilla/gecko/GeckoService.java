/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlarmManager;
import android.app.Service;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import java.io.File;

import org.mozilla.gecko.notifications.ServiceNotificationClient;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.EventCallback;

public class GeckoService extends Service {

    private static final String LOGTAG = "GeckoService";
    private static final boolean DEBUG = false;

    private static final String INTENT_PROFILE_NAME = "org.mozilla.gecko.intent.PROFILE_NAME";
    private static final String INTENT_PROFILE_DIR = "org.mozilla.gecko.intent.PROFILE_DIR";

    private static final String INTENT_ACTION_UPDATE_ADDONS = "update-addons";
    private static final String INTENT_ACTION_CREATE_SERVICES = "create-services";

    private static final String INTENT_SERVICE_CATEGORY = "category";
    private static final String INTENT_SERVICE_DATA = "data";

    private static class EventListener implements NativeEventListener {
        @Override // NativeEventListener
        public void handleMessage(final String event,
                                  final NativeJSObject message,
                                  final EventCallback callback) {
            final Context context = GeckoAppShell.getApplicationContext();
            switch (event) {
            case "Gecko:ScheduleRun":
                if (DEBUG) {
                    Log.d(LOGTAG, "Scheduling " + message.getString("action") +
                                  " @ " + message.getInt("interval") + "ms");
                }

                final Intent intent = getIntentForAction(context, message.getString("action"));
                final PendingIntent pendingIntent = PendingIntent.getService(
                        context, /* requestCode */ 0, intent, PendingIntent.FLAG_CANCEL_CURRENT);

                final AlarmManager am = (AlarmManager)
                    context.getSystemService(Context.ALARM_SERVICE);
                // Cancel any previous alarm and schedule a new one.
                am.setInexactRepeating(AlarmManager.ELAPSED_REALTIME,
                                       message.getInt("trigger"),
                                       message.getInt("interval"),
                                       pendingIntent);
                break;

            default:
                throw new UnsupportedOperationException(event);
            }
        }
    }

    private static final EventListener EVENT_LISTENER = new EventListener();

    public static void register() {
        if (DEBUG) {
            Log.d(LOGTAG, "Registered listener");
        }
        EventDispatcher.getInstance().registerGeckoThreadListener(EVENT_LISTENER,
                "Gecko:ScheduleRun");
    }

    public static void unregister() {
        if (DEBUG) {
            Log.d(LOGTAG, "Unregistered listener");
        }
        EventDispatcher.getInstance().unregisterGeckoThreadListener(EVENT_LISTENER,
                "Gecko:ScheduleRun");
    }

    @Override // Service
    public void onCreate() {
        GeckoAppShell.ensureCrashHandling();
        GeckoAppShell.setApplicationContext(getApplicationContext());
        GeckoAppShell.setNotificationClient(new ServiceNotificationClient(getApplicationContext()));
        GeckoThread.onResume();
        super.onCreate();

        if (DEBUG) {
            Log.d(LOGTAG, "Created");
        }
    }

    @Override // Service
    public void onDestroy() {
        GeckoThread.onPause();

        // We want to block here if we can, so we don't get killed when Gecko is in the
        // middle of handling onPause().
        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            GeckoThread.waitOnGecko();
        }

        if (DEBUG) {
            Log.d(LOGTAG, "Destroyed");
        }
        super.onDestroy();
    }

    private static Intent getIntentForAction(final Context context, final String action) {
        final Intent intent = new Intent(action, /* uri */ null, context, GeckoService.class);
        final GeckoProfile profile = GeckoThread.getActiveProfile();
        if (profile != null) {
            setIntentProfile(intent, profile.getName(), profile.getDir().getAbsolutePath());
        }
        return intent;
    }

    public static Intent getIntentToCreateServices(final Context context, final String category, final String data) {
        final Intent intent = getIntentForAction(context, INTENT_ACTION_CREATE_SERVICES);
        intent.putExtra(INTENT_SERVICE_CATEGORY, category);
        intent.putExtra(INTENT_SERVICE_DATA, data);
        return intent;
    }

    public static Intent getIntentToCreateServices(final Context context, final String category) {
        return getIntentToCreateServices(context, category, /* data */ null);
    }

    public static void setIntentProfile(final Intent intent, final String profileName,
                                        final String profileDir) {
        intent.putExtra(INTENT_PROFILE_NAME, profileName);
        intent.putExtra(INTENT_PROFILE_DIR, profileDir);
    }

    private int handleIntent(final Intent intent, final int startId) {
        if (DEBUG) {
            Log.d(LOGTAG, "Handling " + intent.getAction());
        }

        final String profileName = intent.getStringExtra(INTENT_PROFILE_NAME);
        final String profileDir = intent.getStringExtra(INTENT_PROFILE_DIR);

        if (profileName == null) {
            throw new IllegalArgumentException("Intent must specify profile.");
        }

        if (!GeckoThread.initWithProfile(profileName != null ? profileName : "",
                                         profileDir != null ? new File(profileDir) : null)) {
            Log.w(LOGTAG, "Ignoring due to profile mismatch: " +
                          profileName + " [" + profileDir + ']');

            final GeckoProfile profile = GeckoThread.getActiveProfile();
            if (profile != null) {
                Log.w(LOGTAG, "Current profile is " + profile.getName() +
                              " [" + profile.getDir().getAbsolutePath() + ']');
            }
            stopSelf(startId);
            return Service.START_NOT_STICKY;
        }

        GeckoThread.launch();

        switch (intent.getAction()) {
        case INTENT_ACTION_UPDATE_ADDONS:
            // Run the add-on update service. Because the service is automatically invoked
            // when loading Gecko, we don't have to do anything else here.
            break;

        case INTENT_ACTION_CREATE_SERVICES:
            final String category = intent.getStringExtra(INTENT_SERVICE_CATEGORY);
            final String data = intent.getStringExtra(INTENT_SERVICE_DATA);

            if (category == null) {
                break;
            }
            GeckoThread.createServices(category, data);
            break;

        default:
            Log.w(LOGTAG, "Unknown request: " + intent);
        }

        stopSelf(startId);
        return Service.START_NOT_STICKY;
    }

    @Override // Service
    public int onStartCommand(final Intent intent, final int flags, final int startId) {
        if (intent == null) {
            return Service.START_NOT_STICKY;
        }
        try {
            return handleIntent(intent, startId);
        } catch (final Throwable e) {
            Log.e(LOGTAG, "Cannot handle intent: " + intent, e);
            return Service.START_NOT_STICKY;
        }
    }

    @Override // Service
    public IBinder onBind(final Intent intent) {
        return null;
    }
}
