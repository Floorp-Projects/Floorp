/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Locale;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class GeckoThread extends Thread {
    private static final String LOGTAG = "GeckoThread";

    public enum State {
        // After being loaded by class loader.
        @WrapForJNI INITIAL(0),
        // After launching Gecko thread
        @WrapForJNI LAUNCHED(1),
        // After loading the mozglue library.
        @WrapForJNI MOZGLUE_READY(2),
        // After loading the libxul library.
        @WrapForJNI LIBS_READY(3),
        // After initializing nsAppShell and JNI calls.
        @WrapForJNI JNI_READY(4),
        // After initializing profile and prefs.
        @WrapForJNI PROFILE_READY(5),
        // After initializing frontend JS
        @WrapForJNI RUNNING(6),
        // After leaving Gecko event loop
        @WrapForJNI EXITING(3),
        // After exiting GeckoThread (corresponding to "Gecko:Exited" event)
        @WrapForJNI EXITED(0);

        /* The rank is an arbitrary value reflecting the amount of components or features
         * that are available for use. During startup and up to the RUNNING state, the
         * rank value increases because more components are initialized and available for
         * use. During shutdown and up to the EXITED state, the rank value decreases as
         * components are shut down and become unavailable. EXITING has the same rank as
         * LIBS_READY because both states have a similar amount of components available.
         */
        private final int rank;

        private State(int rank) {
            this.rank = rank;
        }

        public boolean is(final State other) {
            return this == other;
        }

        public boolean isAtLeast(final State other) {
            return this.rank >= other.rank;
        }

        public boolean isAtMost(final State other) {
            return this.rank <= other.rank;
        }

        // Inclusive
        public boolean isBetween(final State min, final State max) {
            return this.rank >= min.rank && this.rank <= max.rank;
        }
    }

    public static final State MIN_STATE = State.INITIAL;
    public static final State MAX_STATE = State.EXITED;

    private static volatile State sState = State.INITIAL;

    private static class QueuedCall {
        public Method method;
        public Object target;
        public Object[] args;
        public State state;

        public QueuedCall(final Method method, final Object target,
                          final Object[] args, final State state) {
            this.method = method;
            this.target = target;
            this.args = args;
            this.state = state;
        }
    }

    private static final int QUEUED_CALLS_COUNT = 16;
    private static final ArrayList<QueuedCall> QUEUED_CALLS = new ArrayList<>(QUEUED_CALLS_COUNT);

    private static GeckoThread sGeckoThread;

    @WrapForJNI
    private static final ClassLoader clsLoader = GeckoThread.class.getClassLoader();
    @WrapForJNI
    private static MessageQueue msgQueue;

    private GeckoProfile mProfile;

    private final String mArgs;
    private final String mAction;
    private final boolean mDebugging;

    GeckoThread(GeckoProfile profile, String args, String action, boolean debugging) {
        mProfile = profile;
        mArgs = args;
        mAction = action;
        mDebugging = debugging;

        setName("Gecko");
    }

    public static boolean init(GeckoProfile profile, String args, String action, boolean debugging) {
        ThreadUtils.assertOnUiThread();
        if (isState(State.INITIAL) && sGeckoThread == null) {
            sGeckoThread = new GeckoThread(profile, args, action, debugging);
            return true;
        }
        return false;
    }

    private static boolean canUseProfile(final Context context, final GeckoProfile profile,
                                         final String profileName, final File profileDir) {
        if (profileDir != null && !profileDir.isDirectory()) {
            return false;
        }

        if (profile == null) {
            // We haven't initialized; any profile is okay as long as we follow the guest mode setting.
            return GuestSession.shouldUse(context) ==
                    GeckoProfile.isGuestProfile(context, profileName, profileDir);
        }

        // We already initialized and have a profile; see if it matches ours.
        try {
            return profileDir == null ? profileName.equals(profile.getName()) :
                    profile.getDir().getCanonicalPath().equals(profileDir.getCanonicalPath());
        } catch (final IOException e) {
            Log.e(LOGTAG, "Cannot compare profile " + profileName);
            return false;
        }
    }

    public static boolean canUseProfile(final String profileName, final File profileDir) {
        if (profileName == null) {
            throw new IllegalArgumentException("Null profile name");
        }
        return canUseProfile(GeckoAppShell.getApplicationContext(), getActiveProfile(),
                             profileName, profileDir);
    }

    public static boolean initWithProfile(final String profileName, final File profileDir) {
        if (profileName == null) {
            throw new IllegalArgumentException("Null profile name");
        }

        final Context context = GeckoAppShell.getApplicationContext();
        final GeckoProfile profile = getActiveProfile();

        if (!canUseProfile(context, profile, profileName, profileDir)) {
            // Profile is incompatible with current profile.
            return false;
        }

        if (profile != null) {
            // We already have a compatible profile.
            return true;
        }

        // We haven't initialized yet; okay to initialize now.
        return init(GeckoProfile.get(context, profileName, profileDir),
                    /* args */ null, /* action */ null, /* debugging */ false);
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
            method.setAccessible(true);
            method.invoke(obj, args);
        } catch (final IllegalAccessException e) {
            throw new IllegalStateException("Unexpected exception", e);
        } catch (final InvocationTargetException e) {
            throw new UnsupportedOperationException("Cannot make call", e.getCause());
        }
    }

    // Queue a call to the given method.
    private static void queueNativeCallLocked(final Class<?> cls, final String methodName,
                                              final Object obj, final Object[] args,
                                              final State state) {
        final ArrayList<Class<?>> argTypes = new ArrayList<>(args.length);
        final ArrayList<Object> argValues = new ArrayList<>(args.length);

        for (int i = 0; i < args.length; i++) {
            if (args[i] instanceof Class) {
                argTypes.add((Class<?>) args[i]);
                argValues.add(args[++i]);
                continue;
            }
            Class<?> argType = args[i].getClass();
            if (argType == Boolean.class) argType = Boolean.TYPE;
            else if (argType == Byte.class) argType = Byte.TYPE;
            else if (argType == Character.class) argType = Character.TYPE;
            else if (argType == Double.class) argType = Double.TYPE;
            else if (argType == Float.class) argType = Float.TYPE;
            else if (argType == Integer.class) argType = Integer.TYPE;
            else if (argType == Long.class) argType = Long.TYPE;
            else if (argType == Short.class) argType = Short.TYPE;
            argTypes.add(argType);
            argValues.add(args[i]);
        }
        final Method method;
        try {
            method = cls.getDeclaredMethod(
                    methodName, argTypes.toArray(new Class<?>[argTypes.size()]));
        } catch (final NoSuchMethodException e) {
            throw new IllegalArgumentException("Cannot find method", e);
        }

        if (!Modifier.isNative(method.getModifiers())) {
            // As a precaution, we disallow queuing non-native methods. Queuing non-native
            // methods is dangerous because the method could end up being called on either
            // the original thread or the Gecko thread depending on timing. Native methods
            // usually handle this by posting an event to the Gecko thread automatically,
            // but there is no automatic mechanism for non-native methods.
            throw new UnsupportedOperationException("Not allowed to queue non-native methods");
        }

        if (isStateAtLeast(state)) {
            invokeMethod(method, obj, argValues.toArray());
            return;
        }

        QUEUED_CALLS.add(new QueuedCall(
                method, obj, argValues.toArray(), state));
    }

    /**
     * Queue a call to the given static method until Gecko is in the given state.
     *
     * @param state The Gecko state in which the native call could be executed.
     *              Default is State.RUNNING, which means this queued call will
     *              run when Gecko is at or after RUNNING state.
     * @param cls Class that declares the static method.
     * @param methodName Name of the static method.
     * @param args Args to call the static method with; to specify a parameter type,
     *             pass in a Class instance first, followed by the value.
     */
    public static void queueNativeCallUntil(final State state, final Class<?> cls,
                                            final String methodName, final Object... args) {
        synchronized (QUEUED_CALLS) {
            queueNativeCallLocked(cls, methodName, null, args, state);
        }
    }

    /**
     * Queue a call to the given static method until Gecko is in the RUNNING state.
     */
    public static void queueNativeCall(final Class<?> cls, final String methodName,
                                       final Object... args) {
        synchronized (QUEUED_CALLS) {
            queueNativeCallLocked(cls, methodName, null, args, State.RUNNING);
        }
    }

    /**
     * Queue a call to the given instance method until Gecko is in the given state.
     *
     * @param state The Gecko state in which the native call could be executed.
     * @param obj Object that declares the instance method.
     * @param methodName Name of the instance method.
     * @param args Args to call the instance method with; to specify a parameter type,
     *             pass in a Class instance first, followed by the value.
     */
    public static void queueNativeCallUntil(final State state, final Object obj,
                                            final String methodName, final Object... args) {
        synchronized (QUEUED_CALLS) {
            queueNativeCallLocked(obj.getClass(), methodName, obj, args, state);
        }
    }

    /**
     * Queue a call to the given instance method until Gecko is in the RUNNING state.
     */
    public static void queueNativeCall(final Object obj, final String methodName,
                                       final Object... args) {
        synchronized (QUEUED_CALLS) {
            queueNativeCallLocked(obj.getClass(), methodName, obj, args, State.RUNNING);
        }
    }

    // Run all queued methods
    private static void flushQueuedNativeCallsLocked(final State state) {
        int lastSkipped = -1;
        for (int i = 0; i < QUEUED_CALLS.size(); i++) {
            final QueuedCall call = QUEUED_CALLS.get(i);
            if (call == null) {
                // We already handled the call.
                continue;
            }
            if (!state.isAtLeast(call.state)) {
                // The call is not ready yet; skip it.
                lastSkipped = i;
                continue;
            }
            // Mark as handled.
            QUEUED_CALLS.set(i, null);

            invokeMethod(call.method, call.target, call.args);
        }
        if (lastSkipped < 0) {
            // We're done here; release the memory
            QUEUED_CALLS.clear();
            QUEUED_CALLS.trimToSize();
        } else if (lastSkipped < QUEUED_CALLS.size() - 1) {
            // We skipped some; free up null entries at the end,
            // but keep all the previous entries for later.
            QUEUED_CALLS.subList(lastSkipped + 1, QUEUED_CALLS.size()).clear();
        }
    }

    private static String initGeckoEnvironment() {
        final Context context = GeckoAppShell.getApplicationContext();
        GeckoLoader.loadMozGlue(context);
        setState(State.MOZGLUE_READY);

        final Locale locale = Locale.getDefault();
        final Resources res = context.getResources();
        if (locale.toString().equalsIgnoreCase("zh_hk")) {
            final Locale mappedLocale = Locale.TRADITIONAL_CHINESE;
            Locale.setDefault(mappedLocale);
            Configuration config = res.getConfiguration();
            config.locale = mappedLocale;
            res.updateConfiguration(config, null);
        }

        String[] pluginDirs = null;
        try {
            pluginDirs = GeckoAppShell.getPluginDirectories();
        } catch (Exception e) {
            Log.w(LOGTAG, "Caught exception getting plugin dirs.", e);
        }

        final String resourcePath = context.getPackageResourcePath();
        GeckoLoader.setupGeckoEnvironment(context, pluginDirs, context.getFilesDir().getPath());

        GeckoLoader.loadSQLiteLibs(context, resourcePath);
        GeckoLoader.loadNSSLibs(context, resourcePath);
        GeckoLoader.loadGeckoLibs(context, resourcePath);
        setState(State.LIBS_READY);

        return resourcePath;
    }

    private static String getTypeFromAction(String action) {
        if (GeckoApp.ACTION_HOMESCREEN_SHORTCUT.equals(action)) {
            return "-bookmark";
        }
        return null;
    }

    private String addCustomProfileArg(String args) {
        String profileArg = "";

        // Make sure a profile exists.
        final GeckoProfile profile = getProfile();
        profile.getDir(); // call the lazy initializer

        // If args don't include the profile, make sure it's included.
        if (args == null || !args.matches(".*\\B-(P|profile)\\s+\\S+.*")) {
            if (profile.isCustomProfile()) {
                profileArg = " -profile " + profile.getDir().getAbsolutePath();
            } else {
                profileArg = " -P " + profile.getName();
            }
        }

        return (args != null ? args : "") + profileArg;
    }

    private String getGeckoArgs(final String apkPath) {
        // argv[0] is the program name, which for us is the package name.
        final Context context = GeckoAppShell.getApplicationContext();
        final StringBuilder args = new StringBuilder(context.getPackageName());
        args.append(" -greomni ").append(apkPath);

        final String userArgs = addCustomProfileArg(mArgs);
        if (userArgs != null) {
            args.append(' ').append(userArgs);
        }

        final String type = getTypeFromAction(mAction);
        if (type != null) {
            args.append(" ").append(type);
        }

        // In un-official builds, we want to load Javascript resources fresh
        // with each build.  In official builds, the startup cache is purged by
        // the buildid mechanism, but most un-official builds don't bump the
        // buildid, so we purge here instead.
        if (!AppConstants.MOZILLA_OFFICIAL) {
            Log.w(LOGTAG, "STARTUP PERFORMANCE WARNING: un-official build: purging the " +
                          "startup (JavaScript) caches.");
            args.append(" -purgecaches");
        }

        return args.toString();
    }

    public static GeckoProfile getActiveProfile() {
        if (sGeckoThread == null) {
            return null;
        }
        final GeckoProfile profile = sGeckoThread.mProfile;
        if (profile != null) {
            return profile;
        }
        return sGeckoThread.getProfile();
    }

    public synchronized GeckoProfile getProfile() {
        if (mProfile == null) {
            final Context context = GeckoAppShell.getApplicationContext();
            mProfile = GeckoProfile.initFromArgs(context, mArgs);
        }
        return mProfile;
    }

    @Override
    public void run() {
        Looper.prepare();
        GeckoThread.msgQueue = Looper.myQueue();
        ThreadUtils.sGeckoThread = this;
        ThreadUtils.sGeckoHandler = new Handler();

        // Preparation for pumpMessageLoop()
        final MessageQueue.IdleHandler idleHandler = new MessageQueue.IdleHandler() {
            @Override public boolean queueIdle() {
                final Handler geckoHandler = ThreadUtils.sGeckoHandler;
                Message idleMsg = Message.obtain(geckoHandler);
                // Use |Message.obj == GeckoHandler| to identify our "queue is empty" message
                idleMsg.obj = geckoHandler;
                geckoHandler.sendMessageAtFrontOfQueue(idleMsg);
                // Keep this IdleHandler
                return true;
            }
        };
        Looper.myQueue().addIdleHandler(idleHandler);

        if (mDebugging) {
            try {
                Thread.sleep(5 * 1000 /* 5 seconds */);
            } catch (final InterruptedException e) {
            }
        }

        final String args = getGeckoArgs(initGeckoEnvironment());

        // This can only happen after the call to initGeckoEnvironment
        // above, because otherwise the JNI code hasn't been loaded yet.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override public void run() {
                registerUiThread();
            }
        });

        Log.w(LOGTAG, "zerdatime " + SystemClock.uptimeMillis() + " - runGecko");

        if (!AppConstants.MOZILLA_OFFICIAL) {
            Log.i(LOGTAG, "RunGecko - args = " + args);
        }

        // And go.
        GeckoLoader.nativeRun(args);

        // And... we're done.
        setState(State.EXITED);

        try {
            final JSONObject msg = new JSONObject();
            msg.put("type", "Gecko:Exited");
            EventDispatcher.getInstance().dispatchEvent(msg, null);
        } catch (final JSONException e) {
            Log.e(LOGTAG, "unable to dispatch event", e);
        }

        // Remove pumpMessageLoop() idle handler
        Looper.myQueue().removeIdleHandler(idleHandler);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean pumpMessageLoop(final Message msg) {
        final Handler geckoHandler = ThreadUtils.sGeckoHandler;

        if (msg.obj == geckoHandler && msg.getTarget() == geckoHandler) {
            // Our "queue is empty" message; see runGecko()
            return false;
        }

        if (msg.getTarget() == null) {
            Looper.myLooper().quit();
        } else {
            msg.getTarget().dispatchMessage(msg);
        }

        return true;
    }

    /**
     * Check that the current Gecko thread state matches the given state.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isState(final State state) {
        return sState.is(state);
    }

    /**
     * Check that the current Gecko thread state is at the given state or further along,
     * according to the order defined in the State enum.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateAtLeast(final State state) {
        return sState.isAtLeast(state);
    }

    /**
     * Check that the current Gecko thread state is at the given state or prior,
     * according to the order defined in the State enum.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateAtMost(final State state) {
        return sState.isAtMost(state);
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
        return sState.isBetween(minState, maxState);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void setState(final State newState) {
        ThreadUtils.assertOnGeckoThread();
        synchronized (QUEUED_CALLS) {
            flushQueuedNativeCallsLocked(newState);
            sState = newState;
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean checkAndSetState(final State currentState, final State newState) {
        synchronized (QUEUED_CALLS) {
            if (sState == currentState) {
                flushQueuedNativeCallsLocked(newState);
                sState = newState;
                return true;
            }
        }
        return false;
    }

    @WrapForJNI(stubName = "SpeculativeConnect")
    private static native void speculativeConnectNative(String uri);

    public static void speculativeConnect(final String uri) {
        // This is almost always called before Gecko loads, so we don't
        // bother checking here if Gecko is actually loaded or not.
        // Speculative connection depends on proxy settings,
        // so the earliest it can happen is after profile is ready.
        queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class,
                             "speculativeConnectNative", uri);
    }

    @WrapForJNI @RobocopTarget
    public static native void waitOnGecko();

    @WrapForJNI(stubName = "OnPause", dispatchTo = "gecko")
    private static native void nativeOnPause();

    public static void onPause() {
        if (isStateAtLeast(State.PROFILE_READY)) {
            nativeOnPause();
        } else {
            queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class,
                                 "nativeOnPause");
        }
    }

    @WrapForJNI(stubName = "OnResume", dispatchTo = "gecko")
    private static native void nativeOnResume();

    public static void onResume() {
        if (isStateAtLeast(State.PROFILE_READY)) {
            nativeOnResume();
        } else {
            queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class,
                                 "nativeOnResume");
        }
    }

    @WrapForJNI(stubName = "CreateServices", dispatchTo = "gecko")
    private static native void nativeCreateServices(String category, String data);

    public static void createServices(final String category, final String data) {
        if (isStateAtLeast(State.PROFILE_READY)) {
            nativeCreateServices(category, data);
        } else {
            queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class, "nativeCreateServices",
                                 String.class, category, String.class, data);
        }
    }

    // Implemented in mozglue/android/APKOpen.cpp.
    /* package */ static native void registerUiThread();
}
