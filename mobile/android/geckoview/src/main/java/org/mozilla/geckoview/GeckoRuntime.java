/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.arch.lifecycle.ProcessLifecycleOwner;
import android.arch.lifecycle.Lifecycle;
import android.arch.lifecycle.LifecycleObserver;
import android.arch.lifecycle.OnLifecycleEvent;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.v4.util.ArrayMap;
import android.util.Log;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoNetworkManager;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.GeckoSystemStateListener;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.ContextUtils;
import org.mozilla.gecko.util.DebugConfig;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.yaml.snakeyaml.error.YAMLException;

import java.io.File;
import java.io.FileNotFoundException;
import java.net.URI;
import java.util.List;
import java.util.Map;

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
    private RuntimeTelemetry mTelemetry;
    private StorageController mStorageController;
    private final WebExtensionController mWebExtensionController;
    private WebPushController mPushController;
    private final ContentBlockingController mContentBlockingController;
    private final LoginStorage.Proxy mLoginStorageProxy;

    private GeckoRuntime() {
        mWebExtensionController = new WebExtensionController(this);
        mContentBlockingController = new ContentBlockingController();
        mLoginStorageProxy = new LoginStorage.Proxy();

        if (sRuntime != null) {
            throw new IllegalStateException("Only one GeckoRuntime instance is allowed");
        }
        sRuntime = this;
    }

    @WrapForJNI
    @UiThread
    private @Nullable static GeckoRuntime getInstance() {
        return sRuntime;
    }


    /**
     * Called by mozilla::dom::ClientOpenWindow to retrieve the window id to use
     * for a ServiceWorkerClients.openWindow() request.
     * @param baseUrl The base Url for the request.
     * @param url Url being requested to be opened in a new window.
     * @return SessionID to use for the request.
     */
    @WrapForJNI(calledFrom = "gecko")
    private static @NonNull GeckoResult<String> serviceWorkerOpenWindow(final @NonNull String baseUrl, final @NonNull String url) {
        if (sRuntime != null && sRuntime.mServiceWorkerDelegate != null) {
            final URI actual = URI.create(baseUrl).resolve(url);
            GeckoResult<String> result = new GeckoResult<>();
            // perform the onOpenWindow call in the UI thread
            ThreadUtils.postToUiThread(() -> {
                sRuntime
                    .mServiceWorkerDelegate
                    .onOpenWindow(actual.toString())
                    .accept( session -> {
                        if (session != null) {
                            if (!session.isOpen()) {
                                result.completeExceptionally(new RuntimeException("Returned GeckoSession must be open."));
                            } else {
                                session.loadUri(actual.toString());
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
                Intent i = new Intent(ACTION_CRASHED, null,
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

    /* package */ boolean init(final @NonNull Context context, final @NonNull GeckoRuntimeSettings settings) {
        if (DEBUG) {
            Log.d(LOGTAG, "init");
        }
        int flags = 0;
        if (settings.getUseMultiprocess()) {
            flags |= GeckoThread.FLAG_PRELOAD_CHILD;
        }

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
            } catch (PackageManager.NameNotFoundException e) {
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
        info.args = settings.getArguments();
        info.extras = settings.getExtras();
        info.flags = flags;

        // Bug 1605454: Temporary change for Fenix experiment that disables webrender
        // Once the experiment ends or experimenter gets implemented in Gecko, this should be removed
        // and replaced by :
        // info.prefs = settings.getPrefsMap();
        final Map<String, Object> prefMap = new ArrayMap<String, Object>();
        prefMap.putAll(settings.getPrefsMap());
        if (info.extras.getInt("forcedisablewebrender") == 1) {
            prefMap.put("gfx.webrender.force-disabled", true);
        }
        info.prefs = prefMap;
        // End of Bug 1605454 hack

        String configFilePath = settings.getConfigFilePath();
        if (configFilePath == null) {
            // Default to /data/local/tmp/$PACKAGE-geckoview-config.yaml if android:debuggable="true"
            // or if this application is the current Android "debug_app", and to not read configuration
            // from a file otherwise.
            if (ContextUtils.isApplicationDebuggable(context) || ContextUtils.isApplicationCurrentDebugApp(context)) {
                configFilePath = String.format(CONFIG_FILE_PATH_TEMPLATE, context.getApplicationInfo().packageName);
            }
        }

        if (configFilePath != null && !configFilePath.isEmpty()) {
            try {
                final DebugConfig debugConfig = DebugConfig.fromFile(new File(configFilePath));
                Log.i(LOGTAG, "Adding debug configuration from: " + configFilePath);
                debugConfig.mergeIntoInitInfo(info);
            } catch (YAMLException e) {
                Log.w(LOGTAG, "Failed to add debug configuration from: " + configFilePath, e);
            } catch (FileNotFoundException e) {
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
        return true;
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
     * Register a {@link WebExtension} that will be run with this GeckoRuntime.
     *
     * <p>At this time, WebExtensions don't have access to any UI element and
     * cannot communicate with the application. Any UI element will be
     * ignored.</p>
     *
     * Example:
     * <pre><code>
     *     runtime.registerWebExtension(new WebExtension(
     *              "resource://android/assets/web_extensions/my_webextension/"))
     *           .exceptionally(ex -&gt; {
     *               Log.e("MyActivity", "Could not register WebExtension", ex);
     *               return null;
     *           });
     *
     *     runtime.registerWebExtension(new WebExtension(
     *              "file:///path/to/web_extension/my_webextension2.xpi",
     *              "mywebextension2@example.com"));
     * </code></pre>
     *
     * To learn more about WebExtensions refer to
     * <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions">
     *    Mozilla/Add-ons/WebExtensions
     * </a>.
     *
     * @param webExtension {@link WebExtension} to register
     *
     * @return A {@link GeckoResult} that will complete when the WebExtension
     * has been installed.
     */
    @UiThread
    public @NonNull GeckoResult<Void> registerWebExtension(
            final @NonNull WebExtension webExtension) {
        final CallbackResult<Void> result = new CallbackResult<Void>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(null);
            }
        };

        final GeckoBundle bundle = new GeckoBundle(3);
        bundle.putString("locationUri", webExtension.location);
        bundle.putString("id", webExtension.id);
        bundle.putBoolean("allowContentMessaging",
                (webExtension.flags & WebExtension.Flags.ALLOW_CONTENT_MESSAGING) > 0);

        mWebExtensionController.registerWebExtension(webExtension);

        EventDispatcher.getInstance().dispatch("GeckoView:RegisterWebExtension",
                bundle, result);

        return result;
    }

    /**
     * Unregisters this WebExtension. After a WebExtension is unregistered all
     * scripts associated with it stop running.
     *
     * @param webExtension {@link WebExtension} to unregister
     *
     * @return A {@link GeckoResult} that will complete when the WebExtension
     * has been unregistered.
     */
    @UiThread
    public @NonNull GeckoResult<Void> unregisterWebExtension(
            final @NonNull WebExtension webExtension) {
        final CallbackResult<Void> result = new CallbackResult<Void>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(null);
            }
        };

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("id", webExtension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:UnregisterWebExtension", bundle, result);

        return result.then(success -> {
            mWebExtensionController.unregisterWebExtension(webExtension);
            return GeckoResult.fromValue(success);
        });
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
     * Set the {@link LoginStorage.Delegate} instance on this runtime.
     * This delegate is required for handling login storage requests.
     *
     * @param delegate The {@link LoginStorage.Delegate} handling login storage
     *                 requests.
     */
    @UiThread
    public void setLoginStorageDelegate(
            final @Nullable LoginStorage.Delegate delegate) {
        ThreadUtils.assertOnUiThread();
        mLoginStorageProxy.setDelegate(delegate);
    }

    /**
     * Get the {@link LoginStorage.Delegate} instance set on this runtime.
     *
     * @return The {@link LoginStorage.Delegate} set on this runtime.
     */
    @UiThread
    public @Nullable LoginStorage.Delegate getLoginStorageDelegate() {
        ThreadUtils.assertOnUiThread();
        return mLoginStorageProxy.getDelegate();
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
    @UiThread
    private void notifyOnShow(final WebNotification notification) {
        ThreadUtils.getUiHandler().post(() -> {
            if (mNotificationDelegate != null) {
                mNotificationDelegate.onShowNotification(notification);
            }
        });
    }

    @WrapForJNI
    @UiThread
    private void notifyOnClose(final WebNotification notification) {
        ThreadUtils.getUiHandler().post(() -> {
            if (mNotificationDelegate != null) {
                mNotificationDelegate.onCloseNotification(notification);
            }
        });
    }

    @AnyThread
    public @NonNull GeckoRuntimeSettings getSettings() {
        return mSettings;
    }

    /* package */ void setPref(final String name, final Object value) {
        PrefsHelper.setPref(name, value, /* flush */ false);
    }

    /**
     * Return the telemetry object for this runtime.
     *
     * @return The telemetry object.
     */
    @UiThread
    public @NonNull RuntimeTelemetry getTelemetry() {
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
    @UiThread
    public @Nullable File getProfileDir() {
        ThreadUtils.assertOnUiThread();
        return GeckoThread.getActiveProfile().getDir();
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
