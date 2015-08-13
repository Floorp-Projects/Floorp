/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import java.io.IOException;
import java.util.Locale;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicReference;

public class GeckoThread extends Thread implements GeckoEventListener {
    private static final String LOGTAG = "GeckoThread";

    public enum State {
        // After being loaded by class loader.
        INITIAL,
        // After launching Gecko thread
        LAUNCHED,
        // After initializing frontend JS (corresponding to "Gecko:Ready" event)
        RUNNING,
        // After leaving Gecko event loop
        EXITING,
        // After exiting GeckoThread (corresponding to "Gecko:Exited" event)
        EXITED;

        public boolean is(final State other) {
            return this == other;
        }

        public boolean isAtLeast(final State other) {
            return ordinal() >= other.ordinal();
        }

        public boolean isAtMost(final State other) {
            return ordinal() <= other.ordinal();
        }

        // Inclusive
        public boolean isBetween(final State min, final State max) {
            final int ord = ordinal();
            return ord >= min.ordinal() && ord <= max.ordinal();
        }
    }

    public static final State MIN_STATE = State.INITIAL;
    public static final State MAX_STATE = State.EXITED;

    private static final AtomicReference<State> sState = new AtomicReference<>(State.INITIAL);

    private static final Queue<GeckoEvent> PENDING_EVENTS = new ConcurrentLinkedQueue<GeckoEvent>();

    private static GeckoThread sGeckoThread;

    private final String mArgs;
    private final String mAction;
    private final String mUri;
    private final boolean mDebugging;

    GeckoThread(String args, String action, String uri, boolean debugging) {
        mArgs = args;
        mAction = action;
        mUri = uri;
        mDebugging = debugging;

        setName("Gecko");
        EventDispatcher.getInstance().registerGeckoThreadListener(this, "Gecko:Ready");
    }

    public static boolean ensureInit(String args, String action, String uri) {
        return ensureInit(args, action, uri, /* debugging */ false);
    }

    public static boolean ensureInit(String args, String action, String uri, boolean debugging) {
        ThreadUtils.assertOnUiThread();
        if (isState(State.INITIAL) && sGeckoThread == null) {
            sGeckoThread = new GeckoThread(args, action, uri, debugging);
            return true;
        }
        return false;
    }

    public static boolean launch() {
        ThreadUtils.assertOnUiThread();
        if (checkAndSetState(State.INITIAL, State.LAUNCHED)) {
            sGeckoThread.start();
            return true;
        }
        return false;
    }

    public static boolean isLaunched() {
        return !isState(State.INITIAL);
    }

    @RobocopTarget
    public static boolean isRunning() {
        return isState(State.RUNNING);
    }

    private String initGeckoEnvironment() {
        final Locale locale = Locale.getDefault();

        final Context context = GeckoAppShell.getContext();
        GeckoLoader.loadMozGlue(context);

        final Resources res = context.getResources();
        if (locale.toString().equalsIgnoreCase("zh_hk")) {
            final Locale mappedLocale = Locale.TRADITIONAL_CHINESE;
            Locale.setDefault(mappedLocale);
            Configuration config = res.getConfiguration();
            config.locale = mappedLocale;
            res.updateConfiguration(config, null);
        }

        String resourcePath = "";
        String[] pluginDirs = null;
        try {
            pluginDirs = GeckoAppShell.getPluginDirectories();
        } catch (Exception e) {
            Log.w(LOGTAG, "Caught exception getting plugin dirs.", e);
        }

        resourcePath = context.getPackageResourcePath();
        GeckoLoader.setupGeckoEnvironment(context, pluginDirs, context.getFilesDir().getPath());

        GeckoLoader.loadSQLiteLibs(context, resourcePath);
        GeckoLoader.loadNSSLibs(context, resourcePath);
        GeckoLoader.loadGeckoLibs(context, resourcePath);
        GeckoJavaSampler.setLibsLoaded();

        return resourcePath;
    }

    private String getTypeFromAction(String action) {
        if (GeckoApp.ACTION_HOMESCREEN_SHORTCUT.equals(action)) {
            return "-bookmark";
        }
        return null;
    }

    private String addCustomProfileArg(String args) {
        String profileArg = "";
        String guestArg = "";
        if (GeckoAppShell.getGeckoInterface() != null) {
            final GeckoProfile profile = GeckoAppShell.getGeckoInterface().getProfile();

            if (profile.inGuestMode()) {
                try {
                    profileArg = " -profile " + profile.getDir().getCanonicalPath();
                } catch (final IOException ioe) {
                    Log.e(LOGTAG, "error getting guest profile path", ioe);
                }

                if (args == null || !args.contains(BrowserApp.GUEST_BROWSING_ARG)) {
                    guestArg = " " + BrowserApp.GUEST_BROWSING_ARG;
                }
            } else if (!GeckoProfile.sIsUsingCustomProfile) {
                // If nothing was passed in the intent, make sure the default profile exists and
                // force Gecko to use the default profile for this activity
                profileArg = " -P " + profile.forceCreate().getName();
            }
        }

        return (args != null ? args : "") + profileArg + guestArg;
    }

    @Override
    public void run() {
        Looper.prepare();
        ThreadUtils.sGeckoThread = this;
        ThreadUtils.sGeckoHandler = new Handler();

        if (mDebugging) {
            try {
                Thread.sleep(5 * 1000 /* 5 seconds */);
            } catch (final InterruptedException e) {
            }
        }

        String path = initGeckoEnvironment();

        // This can only happen after the call to initGeckoEnvironment
        // above, because otherwise the JNI code hasn't been loaded yet.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override public void run() {
                GeckoAppShell.registerJavaUiThread();
            }
        });

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - runGecko");

        String args = addCustomProfileArg(mArgs);
        String type = getTypeFromAction(mAction);

        if (!AppConstants.MOZILLA_OFFICIAL) {
            Log.i(LOGTAG, "RunGecko - args = " + args);
        }
        // and then fire us up
        GeckoAppShell.runGecko(path, args, mUri, type);

        // And... we're done.
        setState(State.EXITED);

        try {
            final JSONObject msg = new JSONObject();
            msg.put("type", "Gecko:Exited");
            EventDispatcher.getInstance().dispatchEvent(msg, null);
        } catch (final JSONException e) {
            Log.e(LOGTAG, "unable to dispatch event", e);
        }
    }

    public static void addPendingEvent(final GeckoEvent e) {
        synchronized (PENDING_EVENTS) {
            if (isState(State.RUNNING)) {
                // We may just have switched to running state.
                GeckoAppShell.notifyGeckoOfEvent(e);
                e.recycle();
            } else {
                // Throws if unable to add the event due to capacity restrictions.
                PENDING_EVENTS.add(e);
            }
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        if ("Gecko:Ready".equals(event)) {
            EventDispatcher.getInstance().unregisterGeckoThreadListener(this, event);

            // Synchronize with addPendingEvent, so that all pending events are sent,
            // in order, before we switch to running state.
            synchronized (PENDING_EVENTS) {
                GeckoEvent e;
                while ((e = PENDING_EVENTS.poll()) != null) {
                    GeckoAppShell.notifyGeckoOfEvent(e);
                    e.recycle();
                }
                setState(State.RUNNING);
            }
        }
    }

    /**
     * Check that the current Gecko thread state matches the given state.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isState(final State state) {
        return sState.get().is(state);
    }

    /**
     * Check that the current Gecko thread state is at the given state or further along,
     * according to the order defined in the State enum.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateAtLeast(final State state) {
        return sState.get().isAtLeast(state);
    }

    /**
     * Check that the current Gecko thread state is at the given state or prior,
     * according to the order defined in the State enum.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateAtMost(final State state) {
        return sState.get().isAtMost(state);
    }

    /**
     * Check that the current Gecko thread state falls into an inclusive range of states,
     * according to the order defined in the State enum.
     *
     * @param minState Lower range of allowable states
     * @param maxState Upper range of allowable states
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateBetween(final State minState, final State maxState) {
        return sState.get().isBetween(minState, maxState);
    }

    private static void setState(final State newState) {
        ThreadUtils.assertOnGeckoThread();
        sState.set(newState);
    }

    private static boolean checkAndSetState(final State currentState, final State newState) {
        if (!sState.compareAndSet(currentState, newState)) {
            return false;
        }
        return true;
    }
}
