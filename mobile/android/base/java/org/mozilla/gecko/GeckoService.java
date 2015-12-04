/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlarmManager;
import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.EventCallback;

public class GeckoService extends IntentService {

    private static final String LOGTAG = "GeckoService";
    private static final boolean DEBUG = false;

    private static class EventListener implements NativeEventListener {

        private PendingIntent getIntentForAction(final Context context, final String action) {
            final Intent intent = new Intent(action, /* uri */ null, context, GeckoService.class);
            return PendingIntent.getService(context, /* requestCode */ 0, intent,
                                            PendingIntent.FLAG_CANCEL_CURRENT);
        }

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
                final AlarmManager am = (AlarmManager)
                    context.getSystemService(Context.ALARM_SERVICE);
                // Cancel any previous alarm and schedule a new one.
                am.setInexactRepeating(AlarmManager.ELAPSED_REALTIME,
                                       message.getInt("trigger"),
                                       message.getInt("interval"),
                                       getIntentForAction(context, message.getString("action")));
                break;

            default:
                throw new UnsupportedOperationException(event);
            }
        }
    }

    private static final EventListener EVENT_LISTENER = new EventListener();

    public GeckoService() {
        super("GeckoService");
    }

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

        super.onCreate();

        if (DEBUG) {
            Log.d(LOGTAG, "Created");
        }
        GeckoThread.ensureInit(/* args */ null, /* action */ null);
        GeckoThread.launch();
    }

    @Override // Service
    public void onDestroy() {
        // TODO: Make sure Gecko is in a somewhat idle state
        // because our process could be killed at anytime.

        if (DEBUG) {
            Log.d(LOGTAG, "Destroyed");
        }
        super.onDestroy();
    }

    @Override // IntentService
    protected void onHandleIntent(final Intent intent) {

        if (intent == null) {
            return;
        }

        // We are on the intent service worker thread.
        if (DEBUG) {
            Log.d(LOGTAG, "Handling " + intent.getAction());
        }

        switch (intent.getAction()) {
        case "update-addons":
            // Run the add-on update service. Because the service is automatically invoked
            // when loading Gecko, we don't have to do anything else here.
            break;

        default:
            Log.w(LOGTAG, "Unknown request: " + intent);
        }
    }
}
