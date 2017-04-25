/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.SystemClock;
import android.util.Log;

import com.squareup.leakcanary.LeakCanary;
import com.squareup.leakcanary.RefWatcher;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.home.HomePanelsManager;
import org.mozilla.gecko.lwt.LightweightTheme;
import org.mozilla.gecko.mdns.MulticastDNSManager;
import org.mozilla.gecko.media.AudioFocusAgent;
import org.mozilla.gecko.media.RemoteManager;
import org.mozilla.gecko.notifications.NotificationClient;
import org.mozilla.gecko.notifications.NotificationHelper;
import org.mozilla.gecko.preferences.DistroSharedPrefsImport;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.PRNGFixes;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.File;
import java.lang.reflect.Method;

public class GeckoApplication extends Application
    implements ContextGetter {
    private static final String LOG_TAG = "GeckoApplication";
    private static final String MEDIA_DECODING_PROCESS_CRASH = "MEDIA_DECODING_PROCESS_CRASH";

    private boolean mInBackground;
    private boolean mPausedGecko;
    private boolean mIsInitialResume;

    private LightweightTheme mLightweightTheme;

    private RefWatcher mRefWatcher;

    public GeckoApplication() {
        super();
    }

    public static RefWatcher getRefWatcher(Context context) {
        GeckoApplication app = (GeckoApplication) context.getApplicationContext();
        return app.mRefWatcher;
    }

    public static void watchReference(Context context, Object object) {
        if (context == null) {
            return;
        }

        getRefWatcher(context).watch(object);
    }

    @Override
    public Context getContext() {
        return this;
    }

    @Override
    public SharedPreferences getSharedPreferences() {
        return GeckoSharedPrefs.forApp(this);
    }

    /**
     * We need to do locale work here, because we need to intercept
     * each hit to onConfigurationChanged.
     */
    @Override
    public void onConfigurationChanged(Configuration config) {
        Log.d(LOG_TAG, "onConfigurationChanged: " + config.locale +
                       ", background: " + mInBackground);

        // Do nothing if we're in the background. It'll simply cause a loop
        // (Bug 936756 Comment 11), and it's not necessary.
        if (mInBackground) {
            super.onConfigurationChanged(config);
            return;
        }

        // Otherwise, correct the locale. This catches some cases that GeckoApp
        // doesn't get a chance to.
        try {
            BrowserLocaleManager.getInstance().correctLocale(this, getResources(), config);
        } catch (IllegalStateException ex) {
            // GeckoApp hasn't started, so we have no ContextGetter in BrowserLocaleManager.
            Log.w(LOG_TAG, "Couldn't correct locale.", ex);
        }

        super.onConfigurationChanged(config);
    }

    public void onActivityPause(GeckoActivityStatus activity) {
        mInBackground = true;

        if ((activity.isFinishing() == false) &&
            (activity.isGeckoActivityOpened() == false)) {
            // Notify Gecko that we are pausing; the cache service will be
            // shutdown, closing the disk cache cleanly. If the android
            // low memory killer subsequently kills us, the disk cache will
            // be left in a consistent state, avoiding costly cleanup and
            // re-creation.
            GeckoThread.onPause();
            mPausedGecko = true;

            final BrowserDB db = BrowserDB.from(this);
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    db.expireHistory(getContentResolver(), BrowserContract.ExpirePriority.NORMAL);
                }
            });

            GeckoNetworkManager.getInstance().stop();
        }
    }

    public void onActivityResume(GeckoActivityStatus activity) {
        if (mIsInitialResume) {
            GeckoBatteryManager.getInstance().start(this);
            GeckoFontScaleListener.getInstance().initialize(this);
            GeckoNetworkManager.getInstance().start(this);
            mIsInitialResume = false;
        } else if (mPausedGecko) {
            GeckoThread.onResume();
            mPausedGecko = false;
            GeckoNetworkManager.getInstance().start(this);
        }

        mInBackground = false;
    }

    @Override
    public void onCreate() {
        Log.i(LOG_TAG, "zerdatime " + SystemClock.elapsedRealtime() +
              " - application start");

        // PRNG is a pseudorandom number generator.
        // We need to apply PRNG Fixes before any use of Java Cryptography Architecture.
        // We make use of various JCA methods in data providers for generating GUIDs, as part of FxA
        // flow and during syncing. Note that this is a no-op for devices running API>18, and so we
        // accept the performance penalty on older devices.
        try {
            PRNGFixes.apply();
        } catch (Exception e) {
            // Not much to be done here: it was weak before, so it's weak now.  Not worth aborting.
            Log.e(LOG_TAG, "Got exception applying PRNGFixes! Cryptographic data produced on this device may be weak. Ignoring.", e);
        }

        mIsInitialResume = true;

        mRefWatcher = LeakCanary.install(this);

        final Context context = getApplicationContext();
        GeckoAppShell.setApplicationContext(context);
        HardwareUtils.init(context);
        FilePicker.init(context);
        DownloadsIntegration.init();
        HomePanelsManager.getInstance().init(context);

        GlobalPageMetadata.getInstance().init();

        // We need to set the notification client before launching Gecko, since Gecko could start
        // sending notifications immediately after startup, which we don't want to lose/crash on.
        GeckoAppShell.setNotificationListener(new NotificationClient(context));
        // This getInstance call will force initialization of the NotificationHelper, but does nothing with the result
        NotificationHelper.getInstance(context).init();

        MulticastDNSManager.getInstance(context).init();

        GeckoService.register();

        EventDispatcher.getInstance().registerBackgroundThreadListener(new EventListener(),
                "Profile:Create");

        super.onCreate();
    }

    public void onDelayedStartup() {
        if (AppConstants.MOZ_ANDROID_GCM) {
            // TODO: only run in main process.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    // It's fine to throw GCM initialization onto a background thread; the registration process requires
                    // network access, so is naturally asynchronous.  This, of course, races against Gecko page load of
                    // content requiring GCM-backed services, like Web Push.  There's nothing to be done here.
                    try {
                        final Class<?> clazz = Class.forName("org.mozilla.gecko.push.PushService");
                        final Method onCreate = clazz.getMethod("onCreate", Context.class);
                        onCreate.invoke(null, getApplicationContext()); // Method is static.
                    } catch (Exception e) {
                        Log.e(LOG_TAG, "Got exception during startup; ignoring.", e);
                        return;
                    }
                }
            });
        }

        GeckoAccessibility.setAccessibilityManagerListeners(this);

        AudioFocusAgent.getInstance().attachToContext(this);

        RemoteManager.setCrashReporter(new RemoteManager.ICrashReporter() {
            public void reportDecodingProcessCrash() {
                Telemetry.addToHistogram(MEDIA_DECODING_PROCESS_CRASH, 1);
            }
        });
    }

    private class EventListener implements BundleEventListener
    {
        private void onProfileCreate(final String name, final String path) {
            // Add everything when we're done loading the distribution.
            final Context context = GeckoApplication.this;
            final GeckoProfile profile = GeckoProfile.get(context, name);
            final Distribution distribution = Distribution.getInstance(context);

            distribution.addOnDistributionReadyCallback(new Distribution.ReadyCallback() {
                @Override
                public void distributionNotFound() {
                    this.distributionFound(null);
                }

                @Override
                public void distributionFound(final Distribution distribution) {
                    Log.d(LOG_TAG, "Running post-distribution task: bookmarks.");
                    // Because we are running in the background, we want to synchronize on the
                    // GeckoProfile instance so that we don't race with main thread operations
                    // such as locking/unlocking/removing the profile.
                    synchronized (profile.getLock()) {
                        distributionFoundLocked(distribution);
                    }
                }

                @Override
                public void distributionArrivedLate(final Distribution distribution) {
                    Log.d(LOG_TAG, "Running late distribution task: bookmarks.");
                    // Recover as best we can.
                    synchronized (profile.getLock()) {
                        distributionArrivedLateLocked(distribution);
                    }
                }

                private void distributionFoundLocked(final Distribution distribution) {
                    // Skip initialization if the profile directory has been removed.
                    if (!(new File(path)).exists()) {
                        return;
                    }

                    final ContentResolver cr = context.getContentResolver();
                    final LocalBrowserDB db = new LocalBrowserDB(profile.getName());

                    // We pass the number of added bookmarks to ensure that the
                    // indices of the distribution and default bookmarks are
                    // contiguous. Because there are always at least as many
                    // bookmarks as there are favicons, we can also guarantee that
                    // the favicon IDs won't overlap.
                    final int offset = distribution == null ? 0 :
                            db.addDistributionBookmarks(cr, distribution, 0);
                    db.addDefaultBookmarks(context, cr, offset);

                    Log.d(LOG_TAG, "Running post-distribution task: android preferences.");
                    DistroSharedPrefsImport.importPreferences(context, distribution);
                }

                private void distributionArrivedLateLocked(final Distribution distribution) {
                    // Skip initialization if the profile directory has been removed.
                    if (!(new File(path)).exists()) {
                        return;
                    }

                    final ContentResolver cr = context.getContentResolver();
                    final LocalBrowserDB db = new LocalBrowserDB(profile.getName());

                    // We assume we've been called very soon after startup, and so our offset
                    // into "Mobile Bookmarks" is the number of bookmarks in the DB.
                    final int offset = db.getCount(cr, "bookmarks");
                    db.addDistributionBookmarks(cr, distribution, offset);

                    Log.d(LOG_TAG, "Running late distribution task: android preferences.");
                    DistroSharedPrefsImport.importPreferences(context, distribution);
                }
            });
        }

        @Override // BundleEventListener
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if ("Profile:Create".equals(event)) {
                onProfileCreate(message.getString("name"),
                                message.getString("path"));
            }
        }
    }

    public boolean isApplicationInBackground() {
        return mInBackground;
    }

    public LightweightTheme getLightweightTheme() {
        return mLightweightTheme;
    }

    public void prepareLightweightTheme() {
        mLightweightTheme = new LightweightTheme(this);
    }
}
