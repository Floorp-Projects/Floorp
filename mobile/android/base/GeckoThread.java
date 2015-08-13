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
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
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
        // After loading the mozglue library.
        MOZGLUE_READY,
        // After loading the libxul library.
        LIBS_READY,
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

    private static class QueuedCall {
        public Method method;
        public Object target;
        public Object[] args;

        public QueuedCall(final Method method, final Object target, final Object[] args) {
            this.method = method;
            this.target = target;
            this.args = args;
        }
    }

    private static final int QUEUED_CALLS_COUNT = 16;
    private static final ArrayList<QueuedCall> QUEUED_CALLS = new ArrayList<>(QUEUED_CALLS_COUNT);

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

    // Invoke the given Method and handle checked Exceptions.
    private static void invokeMethod(final Method method, final Object obj, final Object[] args) {
        try {
            method.invoke(obj, args);
        } catch (final IllegalAccessException e) {
            throw new IllegalStateException("Unexpected exception", e);
        } catch (final InvocationTargetException e) {
            throw new UnsupportedOperationException("Cannot make call", e.getCause());
        }
    }

    // Queue a call to the given method.
    private static void queueNativeCallLocked(final Class<?> cls, final String methodName,
                                              final Object obj, final Object[] args) {
        final Class<?>[] argTypes = new Class<?>[args.length];
        for (int i = 0; i < args.length; i++) {
            Class<?> argType = args[i].getClass();
            if (argType == Boolean.class) argType = Boolean.TYPE;
            else if (argType == Byte.class) argType = Byte.TYPE;
            else if (argType == Character.class) argType = Character.TYPE;
            else if (argType == Double.class) argType = Double.TYPE;
            else if (argType == Float.class) argType = Float.TYPE;
            else if (argType == Integer.class) argType = Integer.TYPE;
            else if (argType == Long.class) argType = Long.TYPE;
            else if (argType == Short.class) argType = Short.TYPE;
            argTypes[i] = argType;
        }
        final Method method;
        try {
            method = cls.getDeclaredMethod(methodName, argTypes);
        } catch (final NoSuchMethodException e) {
            throw new UnsupportedOperationException("Cannot find method", e);
        }

        if (QUEUED_CALLS.size() == 0 && isRunning()) {
            invokeMethod(method, obj, args);
            return;
        }
        QUEUED_CALLS.add(new QueuedCall(method, obj, args));
    }

    /**
     * Queue a call to the given static method until Gecko is in RUNNING state.
     *
     * @param cls Class that declares the static method.
     * @param methodName Name of the static method.
     * @param args Args to call the static method with.
     */
    public static void queueNativeCall(final Class<?> cls, final String methodName,
                                       final Object... args) {
        synchronized (QUEUED_CALLS) {
            queueNativeCallLocked(cls, methodName, null, args);
        }
    }

    /**
     * Queue a call to the given instance method until Gecko is in RUNNING state.
     *
     * @param obj Object that declares the instance method.
     * @param methodName Name of the instance method.
     * @param args Args to call the instance method with.
     */
    public static void queueNativeCall(final Object obj, final String methodName,
                                       final Object... args) {
        synchronized (QUEUED_CALLS) {
            queueNativeCallLocked(obj.getClass(), methodName, obj, args);
        }
    }

    // Run all queued methods
    private static void flushQueuedNativeCalls(final State state) {
        if (!state.is(State.RUNNING)) {
            return;
        }
        synchronized (QUEUED_CALLS) {
            for (QueuedCall call : QUEUED_CALLS) {
                invokeMethod(call.method, call.target, call.args);
            }
            QUEUED_CALLS.clear();
            QUEUED_CALLS.trimToSize();
        }
    }

    private String initGeckoEnvironment() {
        final Locale locale = Locale.getDefault();

        final Context context = GeckoAppShell.getContext();
        GeckoLoader.loadMozGlue(context);
        setState(State.MOZGLUE_READY);

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
        setState(State.LIBS_READY);

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
        flushQueuedNativeCalls(newState);
    }

    private static boolean checkAndSetState(final State currentState, final State newState) {
        if (!sState.compareAndSet(currentState, newState)) {
            return false;
        }
        flushQueuedNativeCalls(newState);
        return true;
    }
}
