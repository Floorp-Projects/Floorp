/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.StringTokenizer;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.process.GeckoProcessManager;
import org.mozilla.gecko.process.GeckoProcessType;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.BuildConfig;
import org.mozilla.geckoview.GeckoResult;

public class GeckoThread extends Thread {
  private static final String LOGTAG = "GeckoThread";

  public enum State implements NativeQueue.State {
    // After being loaded by class loader.
    @WrapForJNI
    INITIAL(0),
    // After launching Gecko thread
    @WrapForJNI
    LAUNCHED(1),
    // After loading the mozglue library.
    @WrapForJNI
    MOZGLUE_READY(2),
    // After loading the libxul library.
    @WrapForJNI
    LIBS_READY(3),
    // After initializing nsAppShell and JNI calls.
    @WrapForJNI
    JNI_READY(4),
    // After initializing profile and prefs.
    @WrapForJNI
    PROFILE_READY(5),
    // After initializing frontend JS
    @WrapForJNI
    RUNNING(6),
    // After granting request to shutdown
    @WrapForJNI
    EXITING(3),
    // After granting request to restart
    @WrapForJNI
    RESTARTING(3),
    // After failed lib extraction due to corrupted APK
    CORRUPT_APK(2),
    // After exiting GeckoThread (corresponding to "Gecko:Exited" event)
    @WrapForJNI
    EXITED(0);

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

  // -1 denotes an invalid or missing File Descriptor
  private static final int INVALID_FD = -1;

  private static final NativeQueue sNativeQueue = new NativeQueue(State.INITIAL, State.RUNNING);

  /* package */ static NativeQueue getNativeQueue() {
    return sNativeQueue;
  }

  public static final State MIN_STATE = State.INITIAL;
  public static final State MAX_STATE = State.EXITED;

  private static final Runnable UI_THREAD_CALLBACK =
      new Runnable() {
        @Override
        public void run() {
          ThreadUtils.assertOnUiThread();
          final long nextDelay = runUiThreadCallback();
          if (nextDelay >= 0) {
            ThreadUtils.getUiHandler().postDelayed(this, nextDelay);
          }
        }
      };

  private static final GeckoThread INSTANCE = new GeckoThread();

  @WrapForJNI private static final ClassLoader clsLoader = GeckoThread.class.getClassLoader();
  @WrapForJNI private static MessageQueue msgQueue;
  @WrapForJNI private static int uiThreadId;

  private static TelemetryUtils.Timer sInitTimer;
  private static LinkedList<StateGeckoResult> sStateListeners = new LinkedList<>();

  // Main process parameters
  public static final int FLAG_DEBUGGING = 1 << 0; // Debugging mode.
  public static final int FLAG_PRELOAD_CHILD = 1 << 1; // Preload child during main thread start.
  public static final int FLAG_ENABLE_NATIVE_CRASHREPORTER =
      1 << 2; // Enable native crash reporting.

  /* package */ static final String EXTRA_ARGS = "args";

  private boolean mInitialized;
  private InitInfo mInitInfo;

  public static final class ParcelFileDescriptors {
    public final @Nullable ParcelFileDescriptor prefs;
    public final @Nullable ParcelFileDescriptor prefMap;
    public final @NonNull ParcelFileDescriptor ipc;
    public final @Nullable ParcelFileDescriptor crashReporter;
    public final @Nullable ParcelFileDescriptor crashAnnotation;

    private ParcelFileDescriptors(final Builder builder) {
      prefs = builder.prefs;
      prefMap = builder.prefMap;
      ipc = builder.ipc;
      crashReporter = builder.crashReporter;
      crashAnnotation = builder.crashAnnotation;
    }

    public FileDescriptors detach() {
      return FileDescriptors.builder()
          .prefs(detach(prefs))
          .prefMap(detach(prefMap))
          .ipc(detach(ipc))
          .crashReporter(detach(crashReporter))
          .crashAnnotation(detach(crashAnnotation))
          .build();
    }

    private static int detach(final ParcelFileDescriptor pfd) {
      if (pfd == null) {
        return INVALID_FD;
      }
      return pfd.detachFd();
    }

    public void close() {
      close(prefs, prefMap, ipc, crashReporter, crashAnnotation);
    }

    private static void close(final ParcelFileDescriptor... pfds) {
      for (final ParcelFileDescriptor pfd : pfds) {
        if (pfd != null) {
          try {
            pfd.close();
          } catch (final IOException ex) {
            // Nothing we can do about this really.
            Log.w(LOGTAG, "Failed to close File Descriptors.", ex);
          }
        }
      }
    }

    public static ParcelFileDescriptors from(final FileDescriptors fds) {
      return ParcelFileDescriptors.builder()
          .prefs(from(fds.prefs))
          .prefMap(from(fds.prefMap))
          .ipc(from(fds.ipc))
          .crashReporter(from(fds.crashReporter))
          .crashAnnotation(from(fds.crashAnnotation))
          .build();
    }

    private static ParcelFileDescriptor from(final int fd) {
      if (fd == INVALID_FD) {
        return null;
      }
      try {
        return ParcelFileDescriptor.fromFd(fd);
      } catch (final IOException ex) {
        throw new RuntimeException(ex);
      }
    }

    public static Builder builder() {
      return new Builder();
    }

    public static class Builder {
      ParcelFileDescriptor prefs;
      ParcelFileDescriptor prefMap;
      ParcelFileDescriptor ipc;
      ParcelFileDescriptor crashReporter;
      ParcelFileDescriptor crashAnnotation;

      private Builder() {}

      public ParcelFileDescriptors build() {
        return new ParcelFileDescriptors(this);
      }

      public Builder prefs(final ParcelFileDescriptor prefs) {
        this.prefs = prefs;
        return this;
      }

      public Builder prefMap(final ParcelFileDescriptor prefMap) {
        this.prefMap = prefMap;
        return this;
      }

      public Builder ipc(final ParcelFileDescriptor ipc) {
        this.ipc = ipc;
        return this;
      }

      public Builder crashReporter(final ParcelFileDescriptor crashReporter) {
        this.crashReporter = crashReporter;
        return this;
      }

      public Builder crashAnnotation(final ParcelFileDescriptor crashAnnotation) {
        this.crashAnnotation = crashAnnotation;
        return this;
      }
    }
  }

  public static final class FileDescriptors {
    final int prefs;
    final int prefMap;
    final int ipc;
    final int crashReporter;
    final int crashAnnotation;

    private FileDescriptors(final Builder builder) {
      prefs = builder.prefs;
      prefMap = builder.prefMap;
      ipc = builder.ipc;
      crashReporter = builder.crashReporter;
      crashAnnotation = builder.crashAnnotation;
    }

    public static Builder builder() {
      return new Builder();
    }

    public static class Builder {
      int prefs = INVALID_FD;
      int prefMap = INVALID_FD;
      int ipc = INVALID_FD;
      int crashReporter = INVALID_FD;
      int crashAnnotation = INVALID_FD;

      private Builder() {}

      public FileDescriptors build() {
        return new FileDescriptors(this);
      }

      public Builder prefs(final int prefs) {
        this.prefs = prefs;
        return this;
      }

      public Builder prefMap(final int prefMap) {
        this.prefMap = prefMap;
        return this;
      }

      public Builder ipc(final int ipc) {
        this.ipc = ipc;
        return this;
      }

      public Builder crashReporter(final int crashReporter) {
        this.crashReporter = crashReporter;
        return this;
      }

      public Builder crashAnnotation(final int crashAnnotation) {
        this.crashAnnotation = crashAnnotation;
        return this;
      }
    }
  }

  public static class InitInfo {
    public final String[] args;
    public final Bundle extras;
    public final int flags;
    public final Map<String, Object> prefs;
    public final String userSerialNumber;

    public final boolean xpcshell;
    public final String outFilePath;

    public final FileDescriptors fds;

    private InitInfo(final Builder builder) {
      final List<String> result = new ArrayList<>(builder.mArgs.length);

      boolean xpcshell = false;
      for (final String argument : builder.mArgs) {
        if ("-xpcshell".equals(argument)) {
          xpcshell = true;
        } else {
          result.add(argument);
        }
      }
      this.xpcshell = xpcshell;

      args = result.toArray(new String[0]);

      extras = builder.mExtras != null ? new Bundle(builder.mExtras) : new Bundle(3);
      flags = builder.mFlags;
      prefs = builder.mPrefs;
      userSerialNumber = builder.mUserSerialNumber;

      outFilePath = xpcshell ? builder.mOutFilePath : null;

      if (builder.mFds != null) {
        fds = builder.mFds;
      } else {
        fds = FileDescriptors.builder().build();
      }
    }

    public static Builder builder() {
      return new Builder();
    }

    public static class Builder {
      private String[] mArgs;
      private Bundle mExtras;
      private int mFlags;
      private Map<String, Object> mPrefs;
      private String mUserSerialNumber;

      private String mOutFilePath;

      private FileDescriptors mFds;

      // Prevent direct instantiation
      private Builder() {}

      public InitInfo build() {
        return new InitInfo(this);
      }

      public Builder args(final String[] args) {
        mArgs = args;
        return this;
      }

      public Builder extras(final Bundle extras) {
        mExtras = extras;
        return this;
      }

      public Builder flags(final int flags) {
        mFlags = flags;
        return this;
      }

      public Builder prefs(final Map<String, Object> prefs) {
        mPrefs = prefs;
        return this;
      }

      public Builder userSerialNumber(final String userSerialNumber) {
        mUserSerialNumber = userSerialNumber;
        return this;
      }

      public Builder outFilePath(final String outFilePath) {
        mOutFilePath = outFilePath;
        return this;
      }

      public Builder fds(final FileDescriptors fds) {
        mFds = fds;
        return this;
      }
    }
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
    return info != null && info.fds.ipc != INVALID_FD;
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
      final Configuration config = res.getConfiguration();
      config.locale = mappedLocale;
      res.updateConfiguration(config, null);
    }

    if (!isChildProcess()) {
      GeckoSystemStateListener.getInstance().initialize(context);
    }

    loadGeckoLibs(context);
  }

  private String[] getMainProcessArgs() {
    final Context context = GeckoAppShell.getApplicationContext();
    final ArrayList<String> args = new ArrayList<>();

    // argv[0] is the program name, which for us is the package name.
    args.add(context.getPackageName());

    if (!mInitInfo.xpcshell) {
      args.add("-greomni");
      args.add(context.getPackageResourcePath());
    }

    if (mInitInfo.args != null) {
      args.addAll(Arrays.asList(mInitInfo.args));
    }

    // Legacy "args" parameter
    final String extraArgs = mInitInfo.extras.getString(EXTRA_ARGS, null);
    if (extraArgs != null) {
      final StringTokenizer st = new StringTokenizer(extraArgs);
      while (st.hasMoreTokens()) {
        args.add(st.nextToken());
      }
    }

    // "argX" parameters
    for (int i = 0; mInitInfo.extras.containsKey("arg" + i); i++) {
      final String arg = mInitInfo.extras.getString("arg" + i);
      args.add(arg);
    }

    return args.toArray(new String[0]);
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

    final ArrayList<String> result = new ArrayList<>();
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
    final MessageQueue.IdleHandler idleHandler =
        new MessageQueue.IdleHandler() {
          @Override
          public boolean queueIdle() {
            final Handler geckoHandler = ThreadUtils.sGeckoHandler;
            final Message idleMsg = Message.obtain(geckoHandler);
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
    } else if ((mInitInfo.flags & FLAG_ENABLE_NATIVE_CRASHREPORTER) != 0
        && BuildConfig.DEBUG_BUILD) {
      env.add(0, "MOZ_CRASHREPORTER=1");
    }

    if (mInitInfo.userSerialNumber != null) {
      env.add(0, "MOZ_ANDROID_USER_SERIAL_NUMBER=" + mInitInfo.userSerialNumber);
    }

    // Start the profiler before even loading mozglue, so we can capture more
    // things that are happening on the JVM side.
    maybeStartGeckoProfiler(env);

    GeckoLoader.loadMozGlue(context);
    setState(State.MOZGLUE_READY);

    final boolean isChildProcess = isChildProcess();

    GeckoLoader.setupGeckoEnvironment(
        context,
        isChildProcess,
        context.getFilesDir().getPath(),
        env,
        mInitInfo.prefs,
        mInitInfo.xpcshell);

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

    final String[] args = isChildProcess ? mInitInfo.args : getMainProcessArgs();

    if ((mInitInfo.flags & FLAG_DEBUGGING) != 0) {
      Log.i(LOGTAG, "RunGecko - args = " + TextUtils.join(" ", args));
    }

    // And go.
    GeckoLoader.nativeRun(
        args,
        mInitInfo.fds.prefs,
        mInitInfo.fds.prefMap,
        mInitInfo.fds.ipc,
        mInitInfo.fds.crashReporter,
        mInitInfo.fds.crashAnnotation,
        isChildProcess ? false : mInitInfo.xpcshell,
        isChildProcess ? null : mInitInfo.outFilePath);

    // And... we're done.
    final boolean restarting = isState(State.RESTARTING);
    setState(State.EXITED);

    final GeckoBundle data = new GeckoBundle(1);
    data.putBoolean("restart", restarting);
    EventDispatcher.getInstance().dispatch("Gecko:Exited", data);

    // Remove pumpMessageLoop() idle handler
    Looper.myQueue().removeIdleHandler(idleHandler);

    if (isChildProcess) {
      // The child process is completely controlled by Gecko so we don't really need to keep
      // it alive after Gecko exits.
      System.exit(0);
    }
  }

  // This may start the gecko profiler early by looking at the environment variables.
  // Refer to the platform side for more information about the environment variables:
  // https://searchfox.org/mozilla-central/rev/2f9eacd9d3d995c937b4251a5557d95d494c9be1/tools/profiler/core/platform.cpp#2969-3072
  private static void maybeStartGeckoProfiler(final @NonNull List<String> env) {
    final String startupEnv = "MOZ_PROFILER_STARTUP=";
    final String intervalEnv = "MOZ_PROFILER_STARTUP_INTERVAL=";
    final String capacityEnv = "MOZ_PROFILER_STARTUP_ENTRIES=";
    boolean isStartupProfiling = false;
    // Putting default values for now, but they can be overwritten.
    // Keep these values in sync with profiler defaults.
    int interval = 1;
    // 8M entries. Keep this in sync with `PROFILER_DEFAULT_STARTUP_ENTRIES`.
    int capacity = 8 * 1024 * 1024;
    // We have a default 8M of entries but user can actually put less entries
    // with environment variables. But even though user can put anything, we
    // have a hard cap on the minimum value count, because if it's lower than
    // this value, profiler could not capture anything meaningful.
    // This value is kept in `scMinimumBufferEntries` variable in the cpp side:
    // https://searchfox.org/mozilla-central/rev/fa7f47027917a186fb2052dee104cd06c21dd76f/tools/profiler/core/platform.cpp#749
    // This number is not clear in the cpp code at first, so lets calculate:
    // scMinimumBufferEntries = scMinimumBufferSize / scBytesPerEntry
    // expands into
    // scMinimumNumberOfChunks * 2 * scExpectedMaximumStackSize / scBytesPerEntry
    // and this is: 4 * 2 * 64 * 1024 / 8 = 65536 (~512 kb)
    final int minCapacity = 65536;

    // Looping the environment variable list to check known variable names.
    for (final String envItem : env) {
      if (envItem == null) {
        continue;
      }

      if (envItem.startsWith(startupEnv)) {
        // Check the environment variable value to see if it's positive.
        final String value = envItem.substring(startupEnv.length());
        if (value.isEmpty() || value.equals("0") || value.equals("n") || value.equals("N")) {
          // ''/'0'/'n'/'N' values mean do not start the startup profiler.
          // There's no need to inspect other environment variables,
          // so let's break out of the loop
          break;
        }

        isStartupProfiling = true;
      } else if (envItem.startsWith(intervalEnv)) {
        // Parse the interval environment variable if present
        final String value = envItem.substring(intervalEnv.length());

        try {
          final int intValue = Integer.parseInt(value);
          interval = Math.max(intValue, interval);
        } catch (final NumberFormatException err) {
          // Failed to parse. Do nothing and just use the default value.
        }
      } else if (envItem.startsWith(capacityEnv)) {
        // Parse the capacity environment variable if present
        final String value = envItem.substring(capacityEnv.length());

        try {
          final int intValue = Integer.parseInt(value);
          // See `scMinimumBufferEntries` variable for this value on the platform side.
          capacity = Math.max(intValue, minCapacity);
        } catch (final NumberFormatException err) {
          // Failed to parse. Do nothing and just use the default value.
        }
      }
    }

    if (isStartupProfiling) {
      GeckoJavaSampler.start(interval, capacity);
    }
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
   * Check that the current Gecko thread state is at the given state or further along, according to
   * the order defined in the State enum.
   *
   * @param state State to check
   * @return True if the current Gecko thread state matches
   */
  public static boolean isStateAtLeast(final State state) {
    return sNativeQueue.getState().isAtLeast(state);
  }

  /**
   * Check that the current Gecko thread state is at the given state or prior, according to the
   * order defined in the State enum.
   *
   * @param state State to check
   * @return True if the current Gecko thread state matches
   */
  public static boolean isStateAtMost(final State state) {
    return state.isAtLeast(sNativeQueue.getState());
  }

  /**
   * Check that the current Gecko thread state falls into an inclusive range of states, according to
   * the order defined in the State enum.
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
  private static boolean checkAndSetState(final State expectedState, final State newState) {
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
    queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class, "speculativeConnectNative", uri);
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
      queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class, "nativeOnPause");
    }
  }

  @WrapForJNI(stubName = "OnResume", dispatchTo = "gecko")
  private static native void nativeOnResume();

  public static void onResume() {
    if (isStateAtLeast(State.PROFILE_READY)) {
      nativeOnResume();
    } else {
      queueNativeCallUntil(State.PROFILE_READY, GeckoThread.class, "nativeOnResume");
    }
  }

  @WrapForJNI(stubName = "CreateServices", dispatchTo = "gecko")
  private static native void nativeCreateServices(String category, String data);

  public static void createServices(final String category, final String data) {
    if (isStateAtLeast(State.PROFILE_READY)) {
      nativeCreateServices(category, data);
    } else {
      queueNativeCallUntil(
          State.PROFILE_READY,
          GeckoThread.class,
          "nativeCreateServices",
          String.class,
          category,
          String.class,
          data);
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

  /** Queue a call to the given static method until Gecko is in the RUNNING state. */
  public static void queueNativeCall(
      final Class<?> cls, final String methodName, final Object... args) {
    sNativeQueue.queueUntilReady(cls, methodName, args);
  }

  /** Queue a call to the given instance method until Gecko is in the RUNNING state. */
  public static void queueNativeCall(
      final Object obj, final String methodName, final Object... args) {
    sNativeQueue.queueUntilReady(obj, methodName, args);
  }

  /** Queue a call to the given instance method until Gecko is in the RUNNING state. */
  public static void queueNativeCallUntil(
      final State state, final Object obj, final String methodName, final Object... args) {
    sNativeQueue.queueUntil(state, obj, methodName, args);
  }

  /** Queue a call to the given static method until Gecko is in the RUNNING state. */
  public static void queueNativeCallUntil(
      final State state, final Class<?> cls, final String methodName, final Object... args) {
    sNativeQueue.queueUntil(state, cls, methodName, args);
  }
}
