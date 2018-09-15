/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.content.Context;
import android.os.Process;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoSystemStateListener;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.StringUtils;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;

public final class GeckoRuntime implements Parcelable {
    private static final String LOGTAG = "GeckoRuntime";
    private static final boolean DEBUG = false;

    /**
     * Intent action sent to the crash handler when a crash is encountered.
     * @see GeckoRuntimeSettings.Builder#crashHandler(Class)
     */
    public static final String ACTION_CRASHED = "org.mozilla.gecko.ACTION_CRASHED";

    /**
     * This is a key for extra data sent with {@link #ACTION_CRASHED}. It refers
     * to a String with the path to a Breakpad minidump file containing information about
     * the crash. Several crash reporters are able to ingest this in a
     * crash report, including <a href="https://sentry.io">Sentry</a>
     * and Mozilla's <a href="https://wiki.mozilla.org/Socorro">Socorro</a>.
     * <br><br>
     * Be aware, the minidump can contain personally identifiable information.
     * Ensure you are obeying all applicable laws and policies before sending
     * this to a remote server.
     * @see GeckoRuntimeSettings.Builder#crashHandler(Class)
     */
    public static final String EXTRA_MINIDUMP_PATH = "minidumpPath";

    /**
     * This is a key for extra data sent with {@link #ACTION_CRASHED}. It refers
     * to a string with the path to a file containing extra metadata about the crash. The file
     * contains key-value pairs in the form
     * <pre>Key=Value</pre>
     * Be aware, it may contain sensitive data such
     * as the URI that was loaded at the time of the crash.
     */
    public static final String EXTRA_EXTRAS_PATH = "extrasPath";

    /**
     * This is a key for extra data sent with {@link #ACTION_CRASHED}. The value is
     * a boolean indicating whether or not the crash dump was succcessfully
     * retrieved. If this is false, the dump file referred to in
     * {@link #EXTRA_MINIDUMP_PATH} may be corrupted or incomplete.
     */
    public static final String EXTRA_MINIDUMP_SUCCESS = "minidumpSuccess";

    /**
     * This is a key for extra data sent with {@link #ACTION_CRASHED}. The value is
     * a boolean indicating whether or not the crash was fatal or not. If true, the
     * main application process was affected by the crash. If false, only an internal
     * process used by Gecko has crashed and the application may be able to recover.
     * @see GeckoSession.ContentDelegate#onCrash(GeckoSession)
     */
    public static final String EXTRA_CRASH_FATAL = "fatal";

    private static GeckoRuntime sDefaultRuntime;

    /**
     * Get the default runtime for the given context.
     * This will create and initialize the runtime with the default settings.
     *
     * Note: Only use this for session-less apps.
     *       For regular apps, use create() instead.
     *
     * @param context An application context for the default runtime.
     * @return The (static) default runtime for the context.
     */
    public static synchronized @NonNull GeckoRuntime getDefault(final @NonNull Context context) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG) {
            Log.d(LOGTAG, "getDefault");
        }
        if (sDefaultRuntime == null) {
            sDefaultRuntime = new GeckoRuntime();
            sDefaultRuntime.attachTo(context);
            sDefaultRuntime.init(context, new GeckoRuntimeSettings());
        }

        return sDefaultRuntime;
    }

    private GeckoRuntimeSettings mSettings;
    private Delegate mDelegate;
    private RuntimeTelemetry mTelemetry;

    /**
     * Attach the runtime to the given context.
     *
     * @param context The new context to attach to.
     */
    public void attachTo(final @NonNull Context context) {
        if (DEBUG) {
            Log.d(LOGTAG, "attachTo " + context.getApplicationContext());
        }
        final Context appContext = context.getApplicationContext();
        if (!appContext.equals(GeckoAppShell.getApplicationContext())) {
            GeckoAppShell.setApplicationContext(appContext);
        }
    }

    private final BundleEventListener mEventListener = new BundleEventListener() {
        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            final Class<?> crashHandler = GeckoRuntime.this.getSettings().mCrashHandler;

            if ("Gecko:Exited".equals(event) && mDelegate != null) {
                mDelegate.onShutdown();
                EventDispatcher.getInstance().unregisterUiThreadListener(mEventListener, "Gecko:Exited");
            } else if ("GeckoView:ContentCrash".equals(event) && crashHandler != null) {
                final Context context = GeckoAppShell.getApplicationContext();
                Intent i = new Intent(ACTION_CRASHED, null,
                        context, crashHandler);
                i.putExtra(EXTRA_MINIDUMP_PATH, message.getString(EXTRA_MINIDUMP_PATH));
                i.putExtra(EXTRA_EXTRAS_PATH, message.getString(EXTRA_EXTRAS_PATH));
                i.putExtra(EXTRA_MINIDUMP_SUCCESS, true);
                i.putExtra(EXTRA_CRASH_FATAL, message.getBoolean(EXTRA_CRASH_FATAL, true));

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(i);
                } else {
                    context.startService(i);
                }
            }
        }
    };

    private static final String getProcessName(Context context) {
        final ActivityManager manager = (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        for (final ActivityManager.RunningAppProcessInfo info : manager.getRunningAppProcesses()) {
            if (info.pid == Process.myPid()) {
                return info.processName;
            }
        }

        return null;
    }

    /* package */ boolean init(final @NonNull Context context, final @NonNull GeckoRuntimeSettings settings) {
        if (DEBUG) {
            Log.d(LOGTAG, "init");
        }
        int flags = 0;
        if (settings.getUseContentProcessHint()) {
            flags |= GeckoThread.FLAG_PRELOAD_CHILD;
        }

        if (settings.getPauseForDebuggerEnabled()) {
            flags |= GeckoThread.FLAG_DEBUGGING;
        }

        final Class<?> crashHandler = settings.getCrashHandler();
        if (crashHandler != null) {
            try {
                final ServiceInfo info = context.getPackageManager().getServiceInfo(new ComponentName(context, crashHandler), 0);
                if (info.processName.equals(getProcessName(context))) {
                    throw new IllegalArgumentException("Crash handler service must run in a separate process");
                }

                EventDispatcher.getInstance().registerUiThreadListener(mEventListener, "GeckoView:ContentCrash");

                flags |= GeckoThread.FLAG_ENABLE_NATIVE_CRASHREPORTER;
            } catch (PackageManager.NameNotFoundException e) {
                throw new IllegalArgumentException("Crash handler must be registered as a service");
            }
        }

        GeckoAppShell.setDisplayDensityOverride(settings.getDisplayDensityOverride());
        GeckoAppShell.setDisplayDpiOverride(settings.getDisplayDpiOverride());
        GeckoAppShell.setScreenSizeOverride(settings.getScreenSizeOverride());
        GeckoAppShell.setCrashHandlerService(settings.getCrashHandler());

        if (!GeckoThread.initMainProcess(/* profile */ null, settings.getArguments(),
                                         settings.getExtras(), flags)) {
            Log.w(LOGTAG, "init failed (could not initiate GeckoThread)");
            return false;
        }

        if (!GeckoThread.launch()) {
            Log.w(LOGTAG, "init failed (GeckoThread already launched)");
            return false;
        }

        mSettings = settings;

        // Bug 1453062 -- the EventDispatcher should really live here (or in GeckoThread)
        EventDispatcher.getInstance().registerUiThreadListener(mEventListener, "Gecko:Exited");

        mSettings.runtime = this;
        mSettings.flush();

        // Initialize the system ClipboardManager by accessing it on the main thread.
        GeckoAppShell.getApplicationContext().getSystemService(Context.CLIPBOARD_SERVICE);

        GeckoSystemStateListener.getInstance().initialize(context);
        return true;
    }

    /**
     * Create a new runtime with default settings and attach it to the given
     * context.
     *
     * Create will throw if there is already an active Gecko instance running,
     * to prevent that, bind the runtime to the process lifetime instead of the
     * activity lifetime.
     *
     * @param context The context of the runtime.
     * @return An initialized runtime.
     */
    public static @NonNull GeckoRuntime create(final @NonNull Context context) {
        return create(context, new GeckoRuntimeSettings());
    }

    /**
     * Create a new runtime with the given settings and attach it to the given
     * context.
     *
     * Create will throw if there is already an active Gecko instance running,
     * to prevent that, bind the runtime to the process lifetime instead of the
     * activity lifetime.
     *
     * @param context The context of the runtime.
     * @param settings The settings for the runtime.
     * @return An initialized runtime.
     */
    public static @NonNull GeckoRuntime create(final @NonNull Context context,
                                               final @NonNull GeckoRuntimeSettings settings) {
        ThreadUtils.assertOnUiThread();
        if (DEBUG) {
            Log.d(LOGTAG, "create " + context);
        }

        final GeckoRuntime runtime = new GeckoRuntime();
        runtime.attachTo(context);

        if (!runtime.init(context, settings)) {
            throw new IllegalStateException("Failed to initialize GeckoRuntime");
        }

        return runtime;
    }

    /**
     * Shutdown the runtime. This will invalidate all attached sessions.
     */
    public void shutdown() {
        if (DEBUG) {
            Log.d(LOGTAG, "shutdown");
        }

        GeckoSystemStateListener.getInstance().shutdown();
        GeckoThread.forceQuit();
    }

    public interface Delegate {
        /**
         * This is called when the runtime shuts down. Any GeckoSession instances that were
         * opened with this instance are now considered closed.
         **/
        void onShutdown();
    }

    /**
     * Set a delegate for receiving callbacks relevant to to this GeckoRuntime.
     *
     * @param delegate an implementation of {@link GeckoRuntime.Delegate}.
     */
    public void setDelegate(final Delegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Returns the current delegate, if any.
     *
     * @return an instance of {@link GeckoRuntime.Delegate} or null if no delegate has been set.
     */
    public @Nullable Delegate getDelegate() {
        return mDelegate;
    }

    public GeckoRuntimeSettings getSettings() {
        return mSettings;
    }

    /* package */ void setPref(final String name, final Object value,
                               boolean override) {
        if (override || !GeckoAppShell.isFennec()) {
            // Override pref on Fennec only when requested to prevent
            // overriding of persistent prefs.
            PrefsHelper.setPref(name, value, /* flush */ false);
        }
    }

    /**
     * Return the telemetry object for this runtime.
     *
     * @return The telemetry object.
     */
    public RuntimeTelemetry getTelemetry() {
        ThreadUtils.assertOnUiThread();

        if (mTelemetry == null) {
            mTelemetry = new RuntimeTelemetry(this);
        }
        return mTelemetry;

    }

    /**
     * Get the profile directory for this runtime. This is where Gecko stores
     * internal data.
     *
     * @return Profile directory
     */
    @NonNull public File getProfileDir() {
        return GeckoThread.getActiveProfile().getDir();
    }

    /**
     * Notify Gecko that the screen orientation has changed.
     */
    public void orientationChanged() {
        GeckoScreenOrientation.getInstance().update();
    }

    /**
     * Notify Gecko that the screen orientation has changed.
     * @param newOrientation The new screen orientation, as retrieved e.g. from the current
     *                       {@link android.content.res.Configuration}.
     */
    public void orientationChanged(int newOrientation) {
        GeckoScreenOrientation.getInstance().update(newOrientation);
    }

    @Override // Parcelable
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    public void writeToParcel(Parcel out, int flags) {
        out.writeParcelable(mSettings, flags);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final Parcel source) {
        mSettings = source.readParcelable(getClass().getClassLoader());
    }

    public static final Parcelable.Creator<GeckoRuntime> CREATOR
        = new Parcelable.Creator<GeckoRuntime>() {
        @Override
        public GeckoRuntime createFromParcel(final Parcel in) {
            final GeckoRuntime runtime = new GeckoRuntime();
            runtime.readFromParcel(in);
            return runtime;
        }

        @Override
        public GeckoRuntime[] newArray(final int size) {
            return new GeckoRuntime[size];
        }
    };
}
