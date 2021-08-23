/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.lifecycle.ProcessLifecycleOwner;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.OnLifecycleEvent;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;
import android.provider.Settings;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoNetworkManager;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.GeckoSystemStateListener;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.DebugConfig;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;

public final class GeckoRuntime implements Parcelable {
    private static final String LOGTAG = "GeckoRuntime";
    private static final boolean DEBUG = false;

    private static final String CONFIG_FILE_PATH_TEMPLATE = "/data/local/tmp/%s-geckoview-config.yaml";

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
     * a boolean indicating whether or not the crash was fatal or not. If true, the
     * main application process was affected by the crash. If false, only an internal
     * process used by Gecko has crashed and the application may be able to recover.
     * @see GeckoSession.ContentDelegate#onCrash(GeckoSession)
     */
    public static final String EXTRA_CRASH_FATAL = "fatal";

    private final class LifecycleListener implements LifecycleObserver {
        private boolean mPaused = false;
        public LifecycleListener() {
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
        void onCreate() {
            Log.d(LOGTAG, "Lifecycle: onCreate");
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_START)
        void onStart() {
            Log.d(LOGTAG, "Lifecycle: onStart");
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
        void onResume() {
            Log.d(LOGTAG, "Lifecycle: onResume");
            if (mPaused) {
                // Do not trigger the first onResume event because it breaks nsAppShell::sPauseCount counter thresholds.
                GeckoThread.onResume();
            }
            mPaused = false;
            // Monitor network status and send change notifications to Gecko
            // while active.
            GeckoNetworkManager.getInstance().start(GeckoAppShell.getApplicationContext());
        }

        @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        void onPause() {
            Log.d(LOGTAG, "Lifecycle: onPause");
            mPaused = true;
            // Stop monitoring network status while inactive.
            GeckoNetworkManager.getInstance().stop();
            GeckoThread.onPause();
        }
    }

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
    @UiThread
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

    private static GeckoRuntime sRuntime;
    private GeckoRuntimeSettings mSettings;
    private Delegate mDelegate;
    private ServiceWorkerDelegate mServiceWorkerDelegate;
    private WebNotificationDelegate mNotificationDelegate;
    private ActivityDelegate mActivityDelegate;
    private StorageController mStorageController;
    private final WebExtensionController mWebExtensionController;
    private WebPushController mPushController;
    private final ContentBlockingController mContentBlockingController;
    private final Autocomplete.StorageProxy mAutocompleteStorageProxy;
    private final ProfilerController mProfilerController;

    private GeckoRuntime() {
        mWebExtensionController = new WebExtensionController(this);
        mContentBlockingController = new ContentBlockingController();
        mAutocompleteStorageProxy = new Autocomplete.StorageProxy();
        mProfilerController = new ProfilerController();

        if (sRuntime != null) {
            throw new IllegalStateException("Only one GeckoRuntime instance is allowed");
        }
        sRuntime = this;
    }

    @WrapForJNI
    @UiThread
    /* package */ @Nullable static GeckoRuntime getInstance() {
        return sRuntime;
    }


    /**
     * Called by mozilla::dom::ClientOpenWindow to retrieve the window id to use
     * for a ServiceWorkerClients.openWindow() request.
     * @param url validated Url being requested to be opened in a new window.
     * @return SessionID to use for the request.
     */
    @WrapForJNI(calledFrom = "gecko")
    private static @NonNull GeckoResult<String> serviceWorkerOpenWindow(final @NonNull String url) {
        if (sRuntime != null && sRuntime.mServiceWorkerDelegate != null) {
            final GeckoResult<String> result = new GeckoResult<>();
            // perform the onOpenWindow call in the UI thread
            ThreadUtils.runOnUiThread(() -> {
                sRuntime
                    .mServiceWorkerDelegate
                    .onOpenWindow(url)
                    .accept( session -> {
                        if (session != null) {
                            if (!session.isOpen()) {
                                result.completeExceptionally(new RuntimeException("Returned GeckoSession must be open."));
                            } else {
                                session.loadUri(url);
                                result.complete(session.getId());
                            }
                        } else {
                            result.complete(null);
                        }
                    });
            });
            return result;
        } else {
            return GeckoResult.fromException(new java.lang.RuntimeException("No available Service Worker delegate."));
        }
    }


    /**
     * Attach the runtime to the given context.
     *
     * @param context The new context to attach to.
     */
    @UiThread
    public void attachTo(final @NonNull Context context) {
        ThreadUtils.assertOnUiThread();
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
            } else if ("GeckoView:ContentCrashReport".equals(event) && crashHandler != null) {
                final Context context = GeckoAppShell.getApplicationContext();
                final Intent i = new Intent(ACTION_CRASHED, null,
                        context, crashHandler);
                i.putExtra(EXTRA_MINIDUMP_PATH, message.getString(EXTRA_MINIDUMP_PATH));
                i.putExtra(EXTRA_EXTRAS_PATH, message.getString(EXTRA_EXTRAS_PATH));
                i.putExtra(EXTRA_CRASH_FATAL, message.getBoolean(EXTRA_CRASH_FATAL, true));

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(i);
                } else {
                    context.startService(i);
                }
            }
        }
    };

    private static String getProcessName(final Context context) {
        final ActivityManager manager =
                (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        final List<ActivityManager.RunningAppProcessInfo> infos =
                manager.getRunningAppProcesses();
        if (infos == null) {
            return null;
        }
        for (final ActivityManager.RunningAppProcessInfo info : infos) {
            if (info.pid == Process.myPid()) {
                return info.processName;
            }
        }

        return null;
    }

    private void setArguments(final Context context, final GeckoThread.InitInfo initInfo,
                              final String[] arguments) {
        final List<String> result = new ArrayList<>(arguments.length);
        for (final String argument : arguments) {
            if ("-xpcshell".equals(argument)) {
                // Only debug builds of the test app can run an xpcshell
                if (!BuildConfig.DEBUG
                        || !"org.mozilla.geckoview.test".equals(
                                context.getApplicationContext().getPackageName())) {
                    throw new IllegalArgumentException("Only the test app can run -xpcshell.");
                }

                initInfo.xpcshell = true;
            } else {
                result.add(argument);
            }
        }

        initInfo.args = result.toArray(new String[]{});
    }

    /* package */ boolean init(final @NonNull Context context, final @NonNull GeckoRuntimeSettings settings) {
        if (DEBUG) {
            Log.d(LOGTAG, "init");
        }
        int flags = GeckoThread.FLAG_PRELOAD_CHILD;

        if (settings.getPauseForDebuggerEnabled()) {
            flags |= GeckoThread.FLAG_DEBUGGING;
        }

        final Class<?> crashHandler = settings.getCrashHandler();
        if (crashHandler != null) {
            try {
                final ServiceInfo info =
                        context.getPackageManager()
                        .getServiceInfo(
                                new ComponentName(context, crashHandler), 0);
                if (info.processName.equals(getProcessName(context))) {
                    throw new IllegalArgumentException(
                            "Crash handler service must run in a separate process");
                }

                EventDispatcher.getInstance().registerUiThreadListener(
                        mEventListener, "GeckoView:ContentCrashReport");

                flags |= GeckoThread.FLAG_ENABLE_NATIVE_CRASHREPORTER;
            } catch (final PackageManager.NameNotFoundException e) {
                throw new IllegalArgumentException(
                        "Crash handler must be registered as a service");
            }
        }

        GeckoAppShell.useMaxScreenDepth(settings.getUseMaxScreenDepth());
        GeckoAppShell.setDisplayDensityOverride(settings.getDisplayDensityOverride());
        GeckoAppShell.setDisplayDpiOverride(settings.getDisplayDpiOverride());
        GeckoAppShell.setScreenSizeOverride(settings.getScreenSizeOverride());
        GeckoAppShell.setCrashHandlerService(settings.getCrashHandler());
        GeckoFontScaleListener.getInstance().attachToContext(context, settings);

        final GeckoThread.InitInfo info = new GeckoThread.InitInfo();
        setArguments(context, info, settings.getArguments());
        if (info.xpcshell) {
            info.outFilePath = settings.getExtras().getString("out_file");
            // Xpcshell tests need multi-e10s to work properly
            settings.setProcessCount(BuildConfig.MOZ_ANDROID_CONTENT_SERVICE_COUNT);
        }
        info.extras = settings.getExtras();
        info.flags = flags;
        info.prefs = settings.getPrefsMap();

        // Older versions have problems with SnakeYaml
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            String configFilePath = settings.getConfigFilePath();
            if (configFilePath == null) {
                // Default to /data/local/tmp/$PACKAGE-geckoview-config.yaml if android:debuggable="true"
                // or if this application is the current Android "debug_app", and to not read configuration
                // from a file otherwise.
                if (isApplicationDebuggable(context) || isApplicationCurrentDebugApp(context)) {
                    configFilePath = String.format(CONFIG_FILE_PATH_TEMPLATE, context.getApplicationInfo().packageName);
                }
            }

            if (configFilePath != null && !configFilePath.isEmpty()) {
                try {
                    final DebugConfig debugConfig = DebugConfig.fromFile(new File(configFilePath));
                    Log.i(LOGTAG, "Adding debug configuration from: " + configFilePath);
                    debugConfig.mergeIntoInitInfo(info);
                } catch (final DebugConfig.ConfigException e) {
                    Log.w(LOGTAG, "Failed to add debug configuration from: " + configFilePath, e);
                } catch (final FileNotFoundException e) {
                }
            }
        }

        if (!GeckoThread.init(info)) {
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

        // Attach and commit settings.
        mSettings.attachTo(this);

        // Initialize the system ClipboardManager by accessing it on the main thread.
        GeckoAppShell.getApplicationContext().getSystemService(Context.CLIPBOARD_SERVICE);

        // Add process lifecycle listener to react to backgrounding events.
        ProcessLifecycleOwner.get().getLifecycle().addObserver(new LifecycleListener());
        mProfilerController.addMarker("GeckoView Initialization START", mProfilerController.getProfilerTime());
        return true;
    }

    private boolean isApplicationDebuggable(final @NonNull Context context) {
        final ApplicationInfo applicationInfo = context.getApplicationInfo();
        return (applicationInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    private boolean isApplicationCurrentDebugApp(final @NonNull Context context) {
        final ApplicationInfo applicationInfo = context.getApplicationInfo();

        final String currentDebugApp;
        if (Build.VERSION.SDK_INT >= 17) {
            currentDebugApp = Settings.Global.getString(context.getContentResolver(),
                    Settings.Global.DEBUG_APP);
        } else {
            currentDebugApp = Settings.System.getString(context.getContentResolver(),
                    Settings.System.DEBUG_APP);
        }
        return applicationInfo.packageName.equals(currentDebugApp);
    }

    /* package */ void setDefaultPrefs(final GeckoBundle prefs) {
        EventDispatcher.getInstance().dispatch("GeckoView:SetDefaultPrefs", prefs);
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
    @UiThread
    public static @NonNull GeckoRuntime create(final @NonNull Context context) {
        ThreadUtils.assertOnUiThread();
        return create(context, new GeckoRuntimeSettings());
    }

    /**
     * Returns a WebExtensionController for this GeckoRuntime.
     *
     * @return an instance of {@link WebExtensionController}.
     */
    @UiThread
    public @NonNull WebExtensionController getWebExtensionController() {
        return mWebExtensionController;
    }

    /**
     * Returns the ContentBlockingController for this GeckoRuntime.
     *
     * @return An instance of {@link ContentBlockingController}.
     */
    @UiThread
    public @NonNull ContentBlockingController getContentBlockingController() {
        return mContentBlockingController;
    }

    /**
     * Returns a ProfilerController for this GeckoRuntime.
     *
     * @return an instance of {@link ProfilerController}.
     */
    @UiThread
    public @NonNull ProfilerController getProfilerController() {
        return mProfilerController;
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
    @UiThread
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
    @AnyThread
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
        @UiThread
        void onShutdown();
    }

    /**
     * Set a delegate for receiving callbacks relevant to to this GeckoRuntime.
     *
     * @param delegate an implementation of {@link GeckoRuntime.Delegate}.
     */
    @UiThread
    public void setDelegate(final @Nullable Delegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
    }

    /**
     * Returns the current delegate, if any.
     *
     * @return an instance of {@link GeckoRuntime.Delegate} or null if no delegate has been set.
     */
    @UiThread
    public @Nullable Delegate getDelegate() {
        return mDelegate;
    }

    /**
     * Set the {@link Autocomplete.StorageDelegate} instance on this runtime.
     * This delegate is required for handling autocomplete storage requests.
     *
     * @param delegate The {@link Autocomplete.StorageDelegate} handling
     *                 autocomplete storage requests.
     */
    @UiThread
    public void setAutocompleteStorageDelegate(
            final @Nullable Autocomplete.StorageDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mAutocompleteStorageProxy.setDelegate(delegate);
    }

    /**
     * Set the {@link Autocomplete.LoginStorageDelegate} instance on this runtime.
     * This delegate is required for handling autocomplete storage requests.
     *
     * @param delegate The {@link Autocomplete.LoginStorageDelegate} handling
     *                 autocomplete storage requests.
     *
     * @deprecated This API has been replaced by
     *             {@link #setAutocompleteStorageDelegate} and
     *             will be removed in GeckoView 93.
     */
    @Deprecated @DeprecationSchedule(version = 93, id = "login-storage")
    @UiThread
    public void setLoginStorageDelegate(
            final @Nullable Autocomplete.LoginStorageDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mAutocompleteStorageProxy.setDelegate(delegate);
    }

    /**
     * Get the {@link Autocomplete.StorageDelegate} instance set on this runtime.
     *
     * @return The {@link Autocomplete.StorageDelegate} set on this runtime.
     */
    @UiThread
    public @Nullable Autocomplete.StorageDelegate getAutocompleteStorageDelegate() {
        ThreadUtils.assertOnUiThread();
        return mAutocompleteStorageProxy.getDelegate();
    }

    /**
     * Get the {@link Autocomplete.LoginStorageDelegate} instance set on this runtime.
     *
     * @return The {@link Autocomplete.LoginStorageDelegate} set on this runtime.
     *
     * @deprecated This API has been replaced by
     *             {@link #getAutocompleteStorageDelegate} and
     *             will be removed in GeckoView 93.
     */
    @Deprecated @DeprecationSchedule(version = 93, id = "login-storage")
    @UiThread
    public @Nullable Autocomplete.LoginStorageDelegate getLoginStorageDelegate() {
        ThreadUtils.assertOnUiThread();
        return (Autocomplete.LoginStorageDelegate)mAutocompleteStorageProxy.getDelegate();
    }

    @UiThread
    public interface ServiceWorkerDelegate {

        /**
         * This is called when a service worker tries to open a new window using client.openWindow()
         * The GeckoView application should provide an open {@link GeckoSession} to open the url.
         *
         * @param url Url which the Service Worker wishes to open in a new window.
         * @return New or existing open {@link GeckoSession} in which to open the requested url.
         *
         * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API">Service Worker API</a>
         * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Clients/openWindow">openWindow()</a>
         */
        @UiThread
        @NonNull
        GeckoResult<GeckoSession> onOpenWindow(@NonNull String url);
    }

    /**
     * Sets the {@link ServiceWorkerDelegate} to be used for Service Worker requests.
     *
     * @param serviceWorkerDelegate An instance of {@link ServiceWorkerDelegate}.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API">Service Worker API</a>
     */
    @UiThread
    public void setServiceWorkerDelegate(final @Nullable ServiceWorkerDelegate serviceWorkerDelegate) {
        mServiceWorkerDelegate = serviceWorkerDelegate;
    }

    /**
     * Gets the {@link ServiceWorkerDelegate} to be used for Service Worker requests.
     *
     * @return the {@link ServiceWorkerDelegate} instance set by {@link #setServiceWorkerDelegate}
     */
    @UiThread
    @Nullable
    public ServiceWorkerDelegate getServiceWorkerDelegate() {
        return mServiceWorkerDelegate;
    }

    /**
     * Sets the delegate to be used for handling Web Notifications.
     *
     * @param delegate An instance of {@link WebNotificationDelegate}.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification">Web Notifications</a>
     */
    @UiThread
    public void setWebNotificationDelegate(final @Nullable WebNotificationDelegate delegate) {
        mNotificationDelegate = delegate;
    }

    @WrapForJNI
    /* package */ boolean usesDarkTheme() {
        switch (getSettings().getPreferredColorScheme()) {
            case GeckoRuntimeSettings.COLOR_SCHEME_SYSTEM:
                return GeckoSystemStateListener.getInstance().isNightMode();
            case GeckoRuntimeSettings.COLOR_SCHEME_DARK:
                return true;
            case GeckoRuntimeSettings.COLOR_SCHEME_LIGHT:
            default:
                return false;
        }
    }

    /**
     * Returns the current WebNotificationDelegate, if any
     *
     * @return an instance of  WebNotificationDelegate or null if no delegate has been set
     */
    @WrapForJNI
    @UiThread
    public @Nullable WebNotificationDelegate getWebNotificationDelegate() {
        return mNotificationDelegate;
    }

    @WrapForJNI
    @AnyThread
    private void notifyOnShow(final WebNotification notification) {
        ThreadUtils.runOnUiThread(() -> {
            if (mNotificationDelegate != null) {
                mNotificationDelegate.onShowNotification(notification);
            }
        });
    }

    @WrapForJNI
    @AnyThread
    private void notifyOnClose(final WebNotification notification) {
        ThreadUtils.runOnUiThread(() -> {
            if (mNotificationDelegate != null) {
                mNotificationDelegate.onCloseNotification(notification);
            }
        });
    }

    /**
     * This is used to allow GeckoRuntime to start activities via the embedding
     * application (and {@link android.app.Activity}). Currently this is used to invoke the
     * Google Play FIDO Activity in order to integrate with the Web Authentication API.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Web_Authentication_API">Web Authentication API</a>
     */
    public interface ActivityDelegate {
        /**
         * Sometimes GeckoView needs the application to perform a
         * {@link android.app.Activity#startActivityForResult(Intent, int)}
         * on its behalf. Implementations of this method should call that based on the information
         * in the passed {@link PendingIntent}, collect the result, and resolve the returned
         * {@link GeckoResult} with that data. If the
         * Activity does not return {@link android.app.Activity#RESULT_OK}, the {@link GeckoResult}
         * must be completed with an exception of your choosing.
         *
         * @param intent The {@link PendingIntent} to launch
         * @return A {@link GeckoResult} that is eventually resolved with the Activity result.
         */
        @UiThread
        @Nullable GeckoResult<Intent> onStartActivityForResult(@NonNull PendingIntent intent);
    }

    /**
     * Set the {@link ActivityDelegate} instance on this runtime.
     * This delegate is used to provide GeckoView support for launching external
     * activities and receiving results from those activities.
     *
     * @param delegate The {@link ActivityDelegate} handling intent launching requests.
     */
    @UiThread
    public void setActivityDelegate(
            final @Nullable ActivityDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mActivityDelegate = delegate;
    }

    /**
     * Get the {@link ActivityDelegate} instance set on this runtime, if any,
     *
     * @return The {@link ActivityDelegate} set on this runtime.
     */
    @UiThread
    public @Nullable ActivityDelegate getActivityDelegate() {
        ThreadUtils.assertOnUiThread();
        return mActivityDelegate;
    }

    @AnyThread
    /* package */ GeckoResult<Intent> startActivityForResult(final @NonNull PendingIntent intent) {
        if (!ThreadUtils.isOnUiThread()) {
            // Delegates expect to be called on the UI thread.
            final GeckoResult<Intent> result = new GeckoResult<>();

            ThreadUtils.runOnUiThread(() -> {
                final GeckoResult<Intent> delegateResult = startActivityForResult(intent);
                if (delegateResult != null) {
                    delegateResult.accept(val -> result.complete(val), e -> result.completeExceptionally(e));
                } else {
                    result.completeExceptionally(new IllegalStateException("No result"));
                }
            });

            return result;
        }

        if (mActivityDelegate == null) {
            return GeckoResult.fromException(new IllegalStateException("No delegate attached"));
        }

        @SuppressLint("WrongThread")
        GeckoResult<Intent> result = mActivityDelegate.onStartActivityForResult(intent);
        if (result == null) {
            result = GeckoResult.fromException(new IllegalStateException("No result"));
        }

        return result;
    }

    @AnyThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public @NonNull GeckoRuntimeSettings getSettings() {
        return mSettings;
    }

    /**
     * Get the profile directory for this runtime. This is where Gecko stores
     * internal data.
     *
     * @deprecated This API is deprecated and is kept here just for compatibility, as of
     *             GeckoView 89 it always returns null.
     * @return Profile directory
     */
    @UiThread
    @Deprecated
    @DeprecationSchedule(id = "get-profile-dir", version = 93)
    public @Nullable File getProfileDir() {
        return null;
    }

    /**
     * Notify Gecko that the screen orientation has changed.
     */
    @UiThread
    public void orientationChanged() {
        ThreadUtils.assertOnUiThread();
        GeckoScreenOrientation.getInstance().update();
    }

    /**
     * Notify Gecko that the device configuration has changed.
     * @param newConfig The new Configuration object,
     *                  {@link android.content.res.Configuration}.
     */
    @UiThread
    public void configurationChanged(final @NonNull Configuration newConfig) {
        ThreadUtils.assertOnUiThread();
        GeckoSystemStateListener.getInstance().updateNightMode(newConfig.uiMode);
    }

    /**
     * Notify Gecko that the screen orientation has changed.
     * @param newOrientation The new screen orientation, as retrieved e.g. from the current
     *                       {@link android.content.res.Configuration}.
     */
    @UiThread
    public void orientationChanged(final int newOrientation) {
        ThreadUtils.assertOnUiThread();
        GeckoScreenOrientation.getInstance().update(newOrientation);
    }


    /**
     * Get the storage controller for this runtime.
     * The storage controller can be used to manage persistent storage data
     * accumulated by {@link GeckoSession}.
     *
     * @return The {@link StorageController} for this instance.
     */
    @UiThread
    public @NonNull StorageController getStorageController() {
        ThreadUtils.assertOnUiThread();

        if (mStorageController == null) {
            mStorageController = new StorageController();
        }
        return mStorageController;
    }

    /**
     * Get the Web Push controller for this runtime.
     * The Web Push controller can be used to allow content
     * to use the Web Push API.
     *
     * @return The {@link WebPushController} for this instance.
     */
    @UiThread
    public @NonNull WebPushController getWebPushController() {
        ThreadUtils.assertOnUiThread();

        if (mPushController == null) {
            mPushController = new WebPushController();
        }

        return mPushController;
    }

    /**
     * Appends notes to crash report.
     * @param notes The application notes to append to the crash report.
     */
    @AnyThread
    public void appendAppNotesToCrashReport(@NonNull final String notes) {
        final String notesWithNewLine = notes + "\n";
        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            GeckoAppShell.nativeAppendAppNotesToCrashReport(notesWithNewLine);
        } else {
            GeckoThread.queueNativeCallUntil(GeckoThread.State.PROFILE_READY, GeckoAppShell.class,
                    "nativeAppendAppNotesToCrashReport", String.class, notesWithNewLine);
        }
        // This function already adds a newline
        GeckoAppShell.appendAppNotesToCrashReport(notes);
    }

    @Override // Parcelable
    @AnyThread
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    @AnyThread
    public void writeToParcel(final Parcel out, final int flags) {
        out.writeParcelable(mSettings, flags);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    @AnyThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public void readFromParcel(final @NonNull Parcel source) {
        mSettings = source.readParcelable(getClass().getClassLoader());
    }

    public static final Parcelable.Creator<GeckoRuntime> CREATOR =
            new Parcelable.Creator<GeckoRuntime>() {
        @Override
        @AnyThread
        public GeckoRuntime createFromParcel(final Parcel in) {
            final GeckoRuntime runtime = new GeckoRuntime();
            runtime.readFromParcel(in);
            return runtime;
        }

        @Override
        @AnyThread
        public GeckoRuntime[] newArray(final int size) {
            return new GeckoRuntime[size];
        }
    };
}
