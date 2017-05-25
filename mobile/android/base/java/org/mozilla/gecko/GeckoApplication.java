/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Process;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Log;

import com.squareup.leakcanary.LeakCanary;
import com.squareup.leakcanary.RefWatcher;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomePanelsManager;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.lwt.LightweightTheme;
import org.mozilla.gecko.mdns.MulticastDNSManager;
import org.mozilla.gecko.media.AudioFocusAgent;
import org.mozilla.gecko.media.RemoteManager;
import org.mozilla.gecko.notifications.NotificationClient;
import org.mozilla.gecko.notifications.NotificationHelper;
import org.mozilla.gecko.preferences.DistroSharedPrefsImport;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.PRNGFixes;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UUIDUtil;

import java.io.File;
import java.lang.reflect.Method;
import java.util.UUID;

public class GeckoApplication extends Application
    implements ContextGetter {
    private static final String LOG_TAG = "GeckoApplication";
    private static final String MEDIA_DECODING_PROCESS_CRASH = "MEDIA_DECODING_PROCESS_CRASH";

    private boolean mInBackground;
    private boolean mPausedGecko;
    private boolean mIsInitialResume;

    private LightweightTheme mLightweightTheme;

    private RefWatcher mRefWatcher;

    private static String sSessionUUID = null;

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

    /**
     * @return The string representation of an UUID that changes on each application startup.
     */
    public static String getSessionUUID() {
        return sSessionUUID;
    }

    public static String addDefaultGeckoArgs(String args) {
        if (!AppConstants.MOZILLA_OFFICIAL) {
            // In un-official builds, we want to load Javascript resources fresh
            // with each build.  In official builds, the startup cache is purged by
            // the buildid mechanism, but most un-official builds don't bump the
            // buildid, so we purge here instead.
            Log.w(LOG_TAG, "STARTUP PERFORMANCE WARNING: un-official build: purging the " +
                           "startup (JavaScript) caches.");
            args = (args != null) ? (args + " -purgecaches") : "-purgecaches";
        }
        return args;
    }

    public static String getDefaultUAString() {
        return HardwareUtils.isTablet() ? AppConstants.USER_AGENT_FENNEC_TABLET :
                                          AppConstants.USER_AGENT_FENNEC_MOBILE;
    }

    public static void shutdown(final Intent restartIntent) {
        ThreadUtils.assertOnUiThread();

        // Wait for Gecko to handle any pause events.
        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            GeckoThread.waitOnGecko();
        }

        if (restartIntent == null) {
            // Exiting, so kill our own process.
            Process.killProcess(Process.myPid());
            return;
        }

        // Restarting, so let Restarter kill us.
        final Context context = GeckoAppShell.getApplicationContext();
        final Intent intent = new Intent();
        intent.setClass(context, Restarter.class)
              .putExtra("pid", Process.myPid())
              .putExtra(Intent.EXTRA_INTENT, restartIntent);
        context.startService(intent);
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

    public void onApplicationBackground() {
        mInBackground = true;

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

    public void onApplicationForeground() {
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

        sSessionUUID = UUID.randomUUID().toString();

        GeckoActivityMonitor.getInstance().initialize(this);

        final Context context = getApplicationContext();
        GeckoAppShell.setApplicationContext(context);
        GeckoAppShell.setGeckoInterface(new GeckoAppShell.GeckoInterface() {
            @Override
            public boolean openUriExternal(final String targetURI, final String mimeType,
                                           final String packageName, final String className,
                                           final String action, final String title) {
                // Default to showing prompt in private browsing to be safe.
                return IntentHelper.openUriExternal(targetURI, mimeType, packageName,
                                                    className, action, title, true);
            }

            @Override
            public String[] getHandlersForMimeType(final String mimeType,
                                                   final String action) {
                final Intent intent = IntentHelper.getIntentForActionString(action);
                if (mimeType != null && mimeType.length() > 0) {
                    intent.setType(mimeType);
                }
                return IntentHelper.getHandlersForIntent(intent);
            }

            @Override
            public String[] getHandlersForURL(final String url, final String action) {
                // May contain the whole URL or just the protocol.
                final Uri uri = url.indexOf(':') >= 0 ? Uri.parse(url)
                                                      : new Uri.Builder().scheme(url).build();
                final Intent intent = IntentHelper.getOpenURIIntent(
                        getApplicationContext(), uri.toString(), "",
                        TextUtils.isEmpty(action) ? Intent.ACTION_VIEW : action, "");
                return IntentHelper.getHandlersForIntent(intent);
            }
        });

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

        final EventListener listener = new EventListener();
        EventDispatcher.getInstance().registerUiThreadListener(listener,
                "Gecko:Exited",
                null);
        EventDispatcher.getInstance().registerBackgroundThreadListener(listener,
                "Profile:Create",
                null);

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

            } else if ("Gecko:Exited".equals(event)) {
                // Gecko thread exited first; shutdown the application.
                final Intent restartIntent;
                if (message.getBoolean("restart")) {
                    restartIntent = new Intent(Intent.ACTION_MAIN);
                    restartIntent.setClassName(GeckoAppShell.getApplicationContext(),
                                               AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
                } else {
                    restartIntent = null;
                }
                shutdown(restartIntent);
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

    // Creates a homescreen shortcut for a web page.
    // This is the entry point from nsIShellService.
    @WrapForJNI(calledFrom = "gecko")
    public static void createShortcut(final String title, final String url) {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        final String manifestUrl = selectedTab.getManifestUrl();

        if (manifestUrl != null) {
            // If a page has associated manifest, lets install it
            final GeckoBundle message = new GeckoBundle();
            message.putInt("iconSize", GeckoAppShell.getPreferredIconSize());
            message.putString("manifestUrl", manifestUrl);
            message.putString("originalUrl", url);
            message.putString("originalTitle", title);
            EventDispatcher.getInstance().dispatch("Browser:LoadManifest", message);
            return;
        }

        createBrowserShortcut(title, url);
    }

    public static void createBrowserShortcut(final String title, final String url) {
      Icons.with(GeckoAppShell.getApplicationContext())
              .pageUrl(url)
              .skipNetwork()
              .skipMemory()
              .forLauncherIcon()
              .build()
              .execute(new IconCallback() {
                  @Override
                  public void onIconResponse(final IconResponse response) {
                      createShortcutWithIcon(title, url, response.getBitmap());
                  }
              });
    }

    /* package */ static void createShortcutWithIcon(final String aTitle, final String aURI,
                                                       final Bitmap aIcon) {
        final Intent shortcutIntent = new Intent();
        shortcutIntent.setAction(GeckoApp.ACTION_HOMESCREEN_SHORTCUT);
        shortcutIntent.setData(Uri.parse(aURI));
        shortcutIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                    AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        createHomescreenIcon(shortcutIntent, aTitle, aURI, aIcon);
    }

    public static void createAppShortcut(final String aTitle, final String aURI,
                                         final String manifestPath, final Bitmap aIcon) {
        final Intent shortcutIntent = new Intent();
        shortcutIntent.setAction(GeckoApp.ACTION_WEBAPP);
        shortcutIntent.setData(Uri.parse(aURI));
        shortcutIntent.putExtra("MANIFEST_PATH", manifestPath);
        shortcutIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                    LauncherActivity.class.getName());
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION,
                              TelemetryContract.Method.CONTEXT_MENU,
                              "pwa_add_to_launcher");
        createHomescreenIcon(shortcutIntent, aTitle, aURI, aIcon);
    }

    private static void createHomescreenIcon(final Intent shortcutIntent, final String aTitle,
                                             final String aURI, final Bitmap aIcon) {
        final Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        intent.putExtra(Intent.EXTRA_SHORTCUT_ICON,
                        getLauncherIcon(aIcon, GeckoAppShell.getPreferredIconSize()));

        if (aTitle != null) {
            intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
        } else {
            intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);
        }

        final Context context = GeckoAppShell.getApplicationContext();
        // Do not allow duplicate items.
        intent.putExtra("duplicate", false);

        intent.setAction("com.android.launcher.action.INSTALL_SHORTCUT");
        context.sendBroadcast(intent);

        // Remember interaction
        final UrlAnnotations urlAnnotations = BrowserDB.from(context).getUrlAnnotations();
        urlAnnotations.insertHomeScreenShortcut(context.getContentResolver(), aURI, true);

        // After shortcut is created, show the mobile desktop.
        ActivityUtils.goToHomeScreen(context);
    }

    private static Bitmap getLauncherIcon(Bitmap aSource, int size) {
        final float[] DEFAULT_LAUNCHER_ICON_HSV = { 32.0f, 1.0f, 1.0f };
        final int kOffset = 6;
        final int kRadius = 5;

        final int insetSize = aSource != null ? size * 2 / 3 : size;

        final Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(bitmap);

        // draw a base color
        final Paint paint = new Paint();
        if (aSource == null) {
            // If we aren't drawing a favicon, just use an orange color.
            paint.setColor(Color.HSVToColor(DEFAULT_LAUNCHER_ICON_HSV));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset),
                                           kRadius, kRadius, paint);
        } else if (aSource.getWidth() >= insetSize || aSource.getHeight() >= insetSize) {
            // Otherwise, if the icon is large enough, just draw it.
            final Rect iconBounds = new Rect(0, 0, size, size);
            canvas.drawBitmap(aSource, null, iconBounds, null);
            return bitmap;
        } else {
            // Otherwise, use the dominant color from the icon +
            // a layer of transparent white to lighten it somewhat.
            final int color = BitmapUtils.getDominantColor(aSource);
            paint.setColor(color);
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset),
                                           kRadius, kRadius, paint);
            paint.setColor(Color.argb(100, 255, 255, 255));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset),
                                 kRadius, kRadius, paint);
        }

        // draw the overlay
        final Context context = GeckoAppShell.getApplicationContext();
        final Bitmap overlay = BitmapUtils.decodeResource(context, R.drawable.home_bg);
        canvas.drawBitmap(overlay, null, new Rect(0, 0, size, size), null);

        // draw the favicon
        if (aSource == null)
            aSource = BitmapUtils.decodeResource(context, R.drawable.home_star);

        // by default, we scale the icon to this size
        final int sWidth = insetSize / 2;
        final int sHeight = sWidth;

        final int halfSize = size / 2;
        canvas.drawBitmap(aSource,
                null,
                new Rect(halfSize - sWidth,
                        halfSize - sHeight,
                        halfSize + sWidth,
                        halfSize + sHeight),
                null);

        return bitmap;
    }
}
