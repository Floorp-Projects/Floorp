/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.process.GeckoProcessManager;
import org.mozilla.gecko.process.GeckoProcessType;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.BuildConfig;
import org.mozilla.geckoview.GeckoResult;

import android.app.ActivityManager;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.Process;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.StringTokenizer;

public class GeckoThread extends Thread {
    private static final String LOGTAG = "GeckoThread";

    public enum State implements NativeQueue.State {
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
        // After granting request to shutdown
        @WrapForJNI EXITING(3),
        // After granting request to restart
        @WrapForJNI RESTARTING(3),
        // After failed lib extraction due to corrupted APK
        CORRUPT_APK(2),
        // After exiting GeckoThread (corresponding to "Gecko:Exited" event)
        @WrapForJNI EXITED(0);

        /* The rank is an arbitrary value reflecting the amount of components or features
         * that are available for use. During startup and up to the RUNNING state, the
         * rank value increases because more components are initialized and available for
         * use. During shutdown and up to the EXITED state, the rank value decreases as
         * components are shut down and become unavailable. EXITING has the same rank as
         * LIBS_READY because both states have a similar amount of components available.
         */
        private final int mRank;

        private State(final int rank) {
            mRank = rank;
        }

        @Override
        public boolean is(final NativeQueue.State other) {
            return this == other;
        }

        @Override
        public boolean isAtLeast(final NativeQueue.State other) {
            if (other instanceof State) {
                return mRank >= ((State) other).mRank;
            }
            return false;
        }

        @Override
        public String toString() {
            return name();
        }
    }

    private static final NativeQueue sNativeQueue =
        new NativeQueue(State.INITIAL, State.RUNNING);

    /* package */ static NativeQueue getNativeQueue() {
        return sNativeQueue;
    }

    public static final State MIN_STATE = State.INITIAL;
    public static final State MAX_STATE = State.EXITED;

    private static final Runnable UI_THREAD_CALLBACK = new Runnable() {
        @Override
        public void run() {
            ThreadUtils.assertOnUiThread();
            long nextDelay = runUiThreadCallback();
            if (nextDelay >= 0) {
                ThreadUtils.getUiHandler().postDelayed(this, nextDelay);
            }
        }
    };

    private static final GeckoThread INSTANCE = new GeckoThread();

    @WrapForJNI
    private static final ClassLoader clsLoader = GeckoThread.class.getClassLoader();
    @WrapForJNI
    private static MessageQueue msgQueue;
    @WrapForJNI
    private static int uiThreadId;

    private static TelemetryUtils.Timer sInitTimer;
    private static LinkedList<StateGeckoResult> sStateListeners = new LinkedList<>();

    // Main process parameters
    public static final int FLAG_DEBUGGING = 1 << 0; // Debugging mode.
    public static final int FLAG_PRELOAD_CHILD = 1 << 1; // Preload child during main thread start.
    public static final int FLAG_ENABLE_NATIVE_CRASHREPORTER = 1 << 2; // Enable native crash reporting.

    /* package */ static final String EXTRA_ARGS = "args";
    private static final String EXTRA_PREFS_FD = "prefsFd";
    private static final String EXTRA_PREF_MAP_FD = "prefMapFd";
    private static final String EXTRA_IPC_FD = "ipcFd";
    private static final String EXTRA_CRASH_FD = "crashFd";
    private static final String EXTRA_CRASH_ANNOTATION_FD = "crashAnnotationFd";

    private boolean mInitialized;
    private InitInfo mInitInfo;

    public static class InitInfo {
        public GeckoProfile profile;
        public String[] args;
        public Bundle extras;
        public int flags;
        public Map<String, Object> prefs;

        public int prefsFd;
        public int prefMapFd;
        public int ipcFd;
        public int crashFd;
        public int crashAnnotationFd;
    }

    private static class StateGeckoResult extends GeckoResult<Void> {
        final State state;
        public StateGeckoResult(final State state) {
            this.state = state;
        }
    }

    GeckoThread() {
        // Request more (virtual) stack space to avoid overflows in the CSS frame
        // constructor. 8 MB matches desktop.
        super(null, null, "Gecko", 8 * 1024 * 1024);
    }

    @WrapForJNI
    private static boolean isChildProcess() {
        final InitInfo info = INSTANCE.mInitInfo;
        return info != null && info.extras.getInt(EXTRA_IPC_FD, -1) != -1;
    }

    public static boolean init(final InitInfo info) {
        return INSTANCE.initInternal(info);
    }

    private synchronized boolean initInternal(final InitInfo info) {
        ThreadUtils.assertOnUiThread();
        uiThreadId = Process.myTid();

        if (mInitialized) {
            return false;
        }

        sInitTimer = new TelemetryUtils.UptimeTimer("GV_STARTUP_RUNTIME_MS");

        mInitInfo = info;

        mInitInfo.extras = (info.extras != null) ? new Bundle(info.extras) : new Bundle(3);

        if (info.prefsFd > 0) {
            mInitInfo.extras.putInt(EXTRA_PREFS_FD, info.prefsFd);
        }

        if (info.prefMapFd > 0) {
            mInitInfo.extras.putInt(EXTRA_PREF_MAP_FD, info.prefMapFd);
        }

        if (info.ipcFd > 0) {
            mInitInfo.extras.putInt(EXTRA_IPC_FD, info.ipcFd);
        }

        if (info.crashFd > 0) {
            mInitInfo.extras.putInt(EXTRA_CRASH_FD, info.crashFd);
        }

        if (info.crashAnnotationFd > 0) {
            mInitInfo.extras.putInt(EXTRA_CRASH_ANNOTATION_FD, info.crashAnnotationFd);
        }

        mInitialized = true;
        notifyAll();
        return true;
    }

    public static boolean launch() {
        ThreadUtils.assertOnUiThread();

        if (checkAndSetState(State.INITIAL, State.LAUNCHED)) {
            INSTANCE.start();
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

    private static void loadGeckoLibs(final Context context) {
        GeckoLoader.loadSQLiteLibs(context);
        GeckoLoader.loadNSSLibs(context);
        GeckoLoader.loadGeckoLibs(context);
        setState(State.LIBS_READY);
    }

    private static void initGeckoEnvironment() {
        final Context context = GeckoAppShell.getApplicationContext();
        final Locale locale = Locale.getDefault();
        final Resources res = context.getResources();
        if (locale.toString().equalsIgnoreCase("zh_hk")) {
            final Locale mappedLocale = Locale.TRADITIONAL_CHINESE;
            Locale.setDefault(mappedLocale);
            Configuration config = res.getConfiguration();
            config.locale = mappedLocale;
            res.updateConfiguration(config, null);
        }

        GeckoSystemStateListener.getInstance().initialize(context);

        loadGeckoLibs(context);
    }

    private String[] getMainProcessArgs() {
        final Context context = GeckoAppShell.getApplicationContext();
        final ArrayList<String> args = new ArrayList<String>();

        // argv[0] is the program name, which for us is the package name.
        args.add(context.getPackageName());
        args.add("-greomni");
        args.add(context.getPackageResourcePath());

        final GeckoProfile profile = getProfile();
        if (profile.isCustomProfile()) {
            args.add("-profile");
            args.add(profile.getDir().getAbsolutePath());
        } else {
            profile.getDir(); // Make sure the profile dir exists.
            args.add("-P");
            args.add(profile.getName());
        }

        if (mInitInfo.args != null) {
            args.addAll(Arrays.asList(mInitInfo.args));
        }

        final String extraArgs = mInitInfo.extras.getString(EXTRA_ARGS, null);
        if (extraArgs != null) {
            final StringTokenizer st = new StringTokenizer(extraArgs);
            while (st.hasMoreTokens()) {
                final String token = st.nextToken();
                if ("-P".equals(token) || "-profile".equals(token)) {
                    // Skip -P and -profile arguments because we added them above.
                    if (st.hasMoreTokens()) {
                        st.nextToken();
                    }
                    continue;
                }
                args.add(token);
            }
        }

        return args.toArray(new String[args.size()]);
    }

    @RobocopTarget
    public static @Nullable GeckoProfile getActiveProfile() {
        return INSTANCE.getProfile();
    }

    public synchronized @Nullable GeckoProfile getProfile() {
        if (!mInitialized) {
            return null;
        }
        if (isChildProcess()) {
            throw new UnsupportedOperationException(
                    "Cannot access profile from child process");
        }
        if (mInitInfo.profile == null) {
            final Context context = GeckoAppShell.getApplicationContext();
            mInitInfo.profile = GeckoProfile.initFromArgs(context,
                    mInitInfo.extras.getString(EXTRA_ARGS, null));
        }
        return mInitInfo.profile;
    }

    public static @Nullable Bundle getActiveExtras() {
        synchronized (INSTANCE) {
            if (!INSTANCE.mInitialized) {
                return null;
            }
            return new Bundle(INSTANCE.mInitInfo.extras);
        }
    }

    public static int getActiveFlags() {
        synchronized (INSTANCE) {
            if (!INSTANCE.mInitialized) {
                return 0;
            }

            return INSTANCE.mInitInfo.flags;
        }
    }

    private static ArrayList<String> getEnvFromExtras(final Bundle extras) {
        if (extras == null) {
            return new ArrayList<>();
        }

        ArrayList<String> result = new ArrayList<>();
        if (extras != null) {
            String env = extras.getString("env0");
            for (int c = 1; env != null; c++) {
                if (BuildConfig.DEBUG) {
                    Log.d(LOGTAG, "env var: " + env);
                }
                result.add(env);
                env = extras.getString("env" + c);
            }
        }

        return result;
    }

    @Override
    public void run() {
        Log.i(LOGTAG, "preparing to run Gecko");

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

        // Wait until initialization before preparing environment.
        synchronized (this) {
            while (!mInitialized) {
                try {
                    wait();
                } catch (final InterruptedException e) {
                }
            }
        }

        final Context context = GeckoAppShell.getApplicationContext();
        final List<String> env = getEnvFromExtras(mInitInfo.extras);

        // In Gecko, the native crash reporter is enabled by default in opt builds, and
        // disabled by default in debug builds.
        if ((mInitInfo.flags & FLAG_ENABLE_NATIVE_CRASHREPORTER) == 0 && !BuildConfig.DEBUG_BUILD) {
            env.add(0, "MOZ_CRASHREPORTER_DISABLE=1");
        } else if ((mInitInfo.flags & FLAG_ENABLE_NATIVE_CRASHREPORTER) != 0 && BuildConfig.DEBUG_BUILD) {
            env.add(0, "MOZ_CRASHREPORTER=1");
        }

        // Very early -- before we load mozglue -- wait for Java debuggers.  This allows to connect
        // a dual/hybrid debugger as well, allowing to debug child processes -- including the
        // mozglue loading process.
        maybeWaitForJavaDebugger(context, env);

        GeckoLoader.loadMozGlue(context);
        setState(State.MOZGLUE_READY);

        GeckoLoader.setupGeckoEnvironment(context, context.getFilesDir().getPath(), env, mInitInfo.prefs);

        initGeckoEnvironment();

        if ((mInitInfo.flags & FLAG_PRELOAD_CHILD) != 0) {
            // Preload the content ("tab") child process.
            GeckoProcessManager.getInstance().preload(GeckoProcessType.CONTENT);
        }

        if ((mInitInfo.flags & FLAG_DEBUGGING) != 0) {
            try {
                Thread.sleep(5 * 1000 /* 5 seconds */);
            } catch (final InterruptedException e) {
            }
        }

        Log.w(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() + " - runGecko");

        final String[] args = isChildProcess() ? mInitInfo.args : getMainProcessArgs();

        if ((mInitInfo.flags & FLAG_DEBUGGING) != 0) {
            Log.i(LOGTAG, "RunGecko - args = " + TextUtils.join(" ", args));
        }

        // And go.
        GeckoLoader.nativeRun(args,
                              mInitInfo.extras.getInt(EXTRA_PREFS_FD, -1),
                              mInitInfo.extras.getInt(EXTRA_PREF_MAP_FD, -1),
                              mInitInfo.extras.getInt(EXTRA_IPC_FD, -1),
                              mInitInfo.extras.getInt(EXTRA_CRASH_FD, -1),
                              mInitInfo.extras.getInt(EXTRA_CRASH_ANNOTATION_FD, -1));

        // And... we're done.
        final boolean restarting = isState(State.RESTARTING);
        setState(State.EXITED);

        final GeckoBundle data = new GeckoBundle(1);
        data.putBoolean("restart", restarting);
        EventDispatcher.getInstance().dispatch("Gecko:Exited", data);

        // Remove pumpMessageLoop() idle handler
        Looper.myQueue().removeIdleHandler(idleHandler);
    }

    private static void maybeWaitForJavaDebugger(final @NonNull Context context, final @NonNull List<String> env) {
        for (final String e : env) {
            if (e == null) {
                continue;
            }

            if (e.equals("MOZ_DEBUG_WAIT_FOR_JAVA_DEBUGGER=1")) {
                if (!isChildProcess()) {
                    final String processName = getProcessName(context);
                    waitForJavaDebugger(processName);
                }
            }

            if (e.startsWith("MOZ_DEBUG_CHILD_WAIT_FOR_JAVA_DEBUGGER=")) {
                String filter = e.substring("MOZ_DEBUG_CHILD_WAIT_FOR_JAVA_DEBUGGER=".length());
                if (isChildProcess()) {
                    final String processName = getProcessName(context);
                    if (processName == null || processName.endsWith(filter)) {
                        waitForJavaDebugger(processName);
                    }
                }
            }
        }
    }

    private static @Nullable String getProcessName(final @NonNull Context context) {
        final int pid = Process.myPid();
        final ActivityManager manager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);

        // This can be quite slow, and it can return null.
        List<ActivityManager.RunningAppProcessInfo> processInfos = manager.getRunningAppProcesses();

        if (processInfos == null) {
            return null;
        }

        for (ActivityManager.RunningAppProcessInfo processInfo : processInfos) {
            if (processInfo.pid == pid) {
                return processInfo.processName;
            }
        }

        return null;
    }

    private static void waitForJavaDebugger(final @Nullable String processName) {
        final int pid = Process.myPid();
        final String processIdentification = (isChildProcess() ? "Child process " : "Main process ") +
                (processName != null ? processName : "<unknown>") +
                " (" + pid + ")";

        if (Debug.isDebuggerConnected()) {
            Log.i(LOGTAG, processIdentification + ": Waiting for Java debugger ... " + " already connected");
            return;
        }

        Log.w(LOGTAG, processIdentification + ": Waiting for Java debugger ...");
        Debug.waitForDebugger();
        Log.w(LOGTAG, processIdentification + ": Waiting for Java debugger ... connected");
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
        return sNativeQueue.getState().is(state);
    }

    /**
     * Check that the current Gecko thread state is at the given state or further along,
     * according to the order defined in the State enum.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateAtLeast(final State state) {
        return sNativeQueue.getState().isAtLeast(state);
    }

    /**
     * Check that the current Gecko thread state is at the given state or prior,
     * according to the order defined in the State enum.
     *
     * @param state State to check
     * @return True if the current Gecko thread state matches
     */
    public static boolean isStateAtMost(final State state) {
        return state.isAtLeast(sNativeQueue.getState());
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
        return isStateAtLeast(minState) && isStateAtMost(maxState);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void setState(final State newState) {
        checkAndSetState(null, newState);
    }

    @WrapForJNI(calledFrom = "gecko")
    private static boolean checkAndSetState(final State expectedState,
                                            final State newState) {
        final boolean result = sNativeQueue.checkAndSetState(expectedState, newState);
        if (result) {
            Log.d(LOGTAG, "State changed to " + newState);

            if (sInitTimer != null && isRunning()) {
                sInitTimer.stop();
                sInitTimer = null;
            }

            notifyStateListeners();
        }
        return result;
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

    @UiThread
    public static GeckoResult<Void> waitForState(final State state) {
        final StateGeckoResult result = new StateGeckoResult(state);
        if (isStateAtLeast(state)) {
            result.complete(null);
            return result;
        }

        synchronized (sStateListeners) {
            sStateListeners.add(result);
        }
        return result;
    }

    private static void notifyStateListeners() {
        synchronized (sStateListeners) {
            final LinkedList<StateGeckoResult> newListeners = new LinkedList<>();
            for (final StateGeckoResult result : sStateListeners) {
                if (!isStateAtLeast(result.state)) {
                    newListeners.add(result);
                    continue;
                }

                result.complete(null);
            }

            sStateListeners = newListeners;
        }
    }

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

    @WrapForJNI(calledFrom = "ui")
    /* package */ static native long runUiThreadCallback();

    @WrapForJNI(dispatchTo = "gecko")
    public static native void forceQuit();

    @WrapForJNI(dispatchTo = "gecko")
    public static native void crash();

    @WrapForJNI
    private static void requestUiThreadCallback(final long delay) {
        ThreadUtils.getUiHandler().postDelayed(UI_THREAD_CALLBACK, delay);
    }

    /**
     * Queue a call to the given static method until Gecko is in the RUNNING state.
     */
    public static void queueNativeCall(final Class<?> cls, final String methodName,
                                       final Object... args) {
        sNativeQueue.queueUntilReady(cls, methodName, args);
    }

    /**
     * Queue a call to the given instance method until Gecko is in the RUNNING state.
     */
    public static void queueNativeCall(final Object obj, final String methodName,
                                       final Object... args) {
        sNativeQueue.queueUntilReady(obj, methodName, args);
    }

    /**
     * Queue a call to the given instance method until Gecko is in the RUNNING state.
     */
    public static void queueNativeCallUntil(final State state, final Object obj, final String methodName,
                                       final Object... args) {
        sNativeQueue.queueUntil(state, obj, methodName, args);
    }

    /**
     * Queue a call to the given static method until Gecko is in the RUNNING state.
     */
    public static void queueNativeCallUntil(final State state, final Class<?> cls, final String methodName,
                                       final Object... args) {
        sNativeQueue.queueUntil(state, cls, methodName, args);
    }
}
