/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.os.Parcel;
import android.os.Parcelable;
import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;

public final class GeckoRuntime implements Parcelable {
    private static final String LOGTAG = "GeckoRuntime";
    private static final boolean DEBUG = false;

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
            sDefaultRuntime.init(new GeckoRuntimeSettings());
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
            if ("Gecko:Exited".equals(event) && mDelegate != null) {
                mDelegate.onShutdown();
                EventDispatcher.getInstance().unregisterUiThreadListener(mEventListener, "Gecko:Exited");
            }
        }
    };

    /* package */ boolean init(final @NonNull GeckoRuntimeSettings settings) {
        if (DEBUG) {
            Log.d(LOGTAG, "init");
        }
        int flags = 0;
        if (settings.getUseContentProcessHint()) {
            flags |= GeckoThread.FLAG_PRELOAD_CHILD;
        }

        if (settings.getNativeCrashReportingEnabled()) {
            flags |= GeckoThread.FLAG_ENABLE_NATIVE_CRASHREPORTER;
        }

        if (settings.getJavaCrashReportingEnabled()) {
            flags |= GeckoThread.FLAG_ENABLE_JAVA_CRASHREPORTER;
        }

        if (settings.getPauseForDebuggerEnabled()) {
            flags |= GeckoThread.FLAG_DEBUGGING;
        }

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

        if (!runtime.init(settings)) {
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
     */
    public File getProfileDir() {
        return GeckoThread.getActiveProfile().getDir();
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
