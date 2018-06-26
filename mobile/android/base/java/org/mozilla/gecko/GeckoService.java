/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.JobIntentService;
import android.util.Log;

import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.io.File;

public abstract class GeckoService extends JobIntentService {

    private static final String LOGTAG = "GeckoService";
    private static final boolean DEBUG = false;

    private static final String INTENT_PROFILE_NAME = "org.mozilla.gecko.intent.PROFILE_NAME";
    private static final String INTENT_PROFILE_DIR = "org.mozilla.gecko.intent.PROFILE_DIR";

    protected static final String INTENT_ACTION_CREATE_SERVICES = "create-services";
    protected static final String INTENT_ACTION_LOAD_LIBS = "load-libs";
    protected static final String INTENT_ACTION_START_GECKO = "start-gecko";

    protected static final String INTENT_SERVICE_CATEGORY = "category";
    protected static final String INTENT_SERVICE_DATA = "data";

    private static class EventListener implements BundleEventListener {
        @Override // BundleEventListener
        public void handleMessage(final String event,
                                  final GeckoBundle message,
                                  final EventCallback callback) {
            final Context context = GeckoAppShell.getApplicationContext();
            switch (event) {
                case "Gecko:ScheduleRun":
                    if (DEBUG) {
                        Log.d(LOGTAG, "Scheduling " + message.getString("action") +
                                " @ " + message.getInt("interval") + "ms");
                    }

                    final Intent intent = getIntentForAction(message.getString("action"));
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

    @Override
    protected abstract void onHandleWork(@NonNull Intent intent);

    @Override
    public boolean onStopCurrentWork() {
        return false;   // do not restart if system stopped us before completing our work
    }

    private static Intent getIntentForAction(final String action) {
        final Intent intent = new Intent(action);
        final Bundle extras = GeckoThread.getActiveExtras();
        if (extras != null && extras.size() > 0) {
            intent.replaceExtras(extras);
        }

        final GeckoProfile profile = GeckoThread.getActiveProfile();
        if (profile != null) {
            setIntentProfile(intent, profile.getName(), profile.getDir().getAbsolutePath());
        }
        return intent;
    }

    public static Intent getIntentToCreateServices(final String category, final String data) {
        final Intent intent = getIntentForAction(INTENT_ACTION_CREATE_SERVICES);
        intent.putExtra(INTENT_SERVICE_CATEGORY, category);
        intent.putExtra(INTENT_SERVICE_DATA, data);
        return intent;
    }

    public static Intent getIntentToCreateServices(final String category) {
        return getIntentToCreateServices(category, /* data */ null);
    }

    public static Intent getIntentToLoadLibs() {
        return getIntentForAction(INTENT_ACTION_LOAD_LIBS);
    }

    public static Intent getIntentToStartGecko() {
        return getIntentForAction(INTENT_ACTION_START_GECKO);
    }

    public static void setIntentProfile(final Intent intent, final String profileName,
                                        final String profileDir) {
        intent.putExtra(INTENT_PROFILE_NAME, profileName);
        intent.putExtra(INTENT_PROFILE_DIR, profileDir);
    }

    protected boolean isStartingIntentValid(@NonNull final Intent intent, @NonNull final String expectedAction) {
        final String action = intent.getAction();

        if (action == null) {
            Log.w(LOGTAG, "Unknown request. No action to act on.");
            return false;
        }

        if (!action.equals(expectedAction)) {
            Log.w(LOGTAG, String.format("Unknown request: \"%s\"", action));
            return false;
        }

        if (DEBUG) {
            Log.d(LOGTAG, "Handling " + intent.getAction());
        }

        return true;
    }

    protected boolean initGecko(final Intent intent) {
        if (INTENT_ACTION_LOAD_LIBS.equals(intent.getAction())) {
            // Intentionally not initialize Gecko when only loading libs.
            return true;
        }

        final String profileName = intent.getStringExtra(INTENT_PROFILE_NAME);
        final String profileDir = intent.getStringExtra(INTENT_PROFILE_DIR);

        if (profileName == null) {
            throw new IllegalArgumentException("Intent must specify profile.");
        }

        if (GeckoApplication.getRuntime() != null) {
            // Gecko has already been initialized, make sure it's using the
            // expected profile.
            return GeckoThread.canUseProfile(profileName,
                    profileDir != null ? new File(profileDir) : null);
        }

        String args;
        if (profileDir != null) {
            args = "-profile " + profileDir;
        } else {
            args = "-P " + profileName;
        }

        intent.putExtra(GeckoThread.EXTRA_ARGS, args);
        GeckoApplication.createRuntime(this, new SafeIntent(intent));
        return true;
    }
}
