/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.Manifest;
import android.app.Activity;
import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Environment;
import android.os.Process;
import android.os.SystemClock;
import android.provider.MediaStore;
import android.support.design.widget.Snackbar;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import com.squareup.leakcanary.LeakCanary;
import com.squareup.leakcanary.RefWatcher;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.LocalBrowserDB;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.home.HomePanelsManager;
import org.mozilla.gecko.icons.IconCallback;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.lwt.LightweightTheme;
import org.mozilla.gecko.mdns.MulticastDNSManager;
import org.mozilla.gecko.media.AudioFocusAgent;
import org.mozilla.gecko.notifications.NotificationClient;
import org.mozilla.gecko.notifications.NotificationHelper;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.preferences.DistroSharedPrefsImport;
import org.mozilla.gecko.pwa.PwaUtils;
import org.mozilla.gecko.telemetry.TelemetryBackgroundReceiver;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.BitmapUtils;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.PRNGFixes;
import org.mozilla.gecko.util.ShortcutUtils;
import org.mozilla.gecko.util.ThreadUtils;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.net.URL;
import java.util.UUID;

public class GeckoApplication extends Application
                              implements HapticFeedbackDelegate {
    private static final String LOG_TAG = "GeckoApplication";
    private static final String MEDIA_DECODING_PROCESS_CRASH = "MEDIA_DECODING_PROCESS_CRASH";

    private boolean mInBackground;
    private boolean mPausedGecko;
    private boolean mIsInitialResume;

    private LightweightTheme mLightweightTheme;

    private RefWatcher mRefWatcher;

    private final EventListener mListener = new EventListener();

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

        final Context oldContext = GeckoAppShell.getApplicationContext();
        if (oldContext instanceof GeckoApplication) {
            ((GeckoApplication) oldContext).onDestroy();
        }

        final Context context = getApplicationContext();
        GeckoAppShell.ensureCrashHandling();
        GeckoAppShell.setApplicationContext(context);

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
        MemoryMonitor.getInstance().init(this);

        GeckoAppShell.setHapticFeedbackDelegate(this);
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

        TelemetryBackgroundReceiver.getInstance().init(context);

        // We need to set the notification client before launching Gecko, since Gecko could start
        // sending notifications immediately after startup, which we don't want to lose/crash on.
        GeckoAppShell.setNotificationListener(new NotificationClient(context));
        // This getInstance call will force initialization of the NotificationHelper, but does nothing with the result
        NotificationHelper.getInstance(context).init();

        MulticastDNSManager.getInstance(context).init();

        GeckoService.register();

        IntentHelper.init();

        EventDispatcher.getInstance().registerGeckoThreadListener(mListener,
                "Distribution:GetDirectories",
                null);
        EventDispatcher.getInstance().registerUiThreadListener(mListener,
                "Gecko:Exited",
                "RuntimePermissions:Check",
                "Snackbar:Show",
                "Share:Text",
                null);
        EventDispatcher.getInstance().registerBackgroundThreadListener(mListener,
                "Bookmark:Insert",
                "Image:SetAs",
                "Profile:Create",
                null);

        super.onCreate();
    }

    /**
     * May be called when a new GeckoApplication object
     * replaces an old one due to assets change.
     */
    private void onDestroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(mListener,
                "Distribution:GetDirectories",
                null);
        EventDispatcher.getInstance().unregisterUiThreadListener(mListener,
                "Gecko:Exited",
                "RuntimePermissions:Check",
                "Snackbar:Show",
                "Share:Text",
                null);
        EventDispatcher.getInstance().unregisterBackgroundThreadListener(mListener,
                "Bookmark:Insert",
                "Image:SetAs",
                "Profile:Create",
                null);

        GeckoService.unregister();
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

            } else if ("RuntimePermissions:Check".equals(event)) {
                final String[] permissions = message.getStringArray("permissions");
                final boolean shouldPrompt = message.getBoolean("shouldPrompt", false);
                final Activity currentActivity =
                        GeckoActivityMonitor.getInstance().getCurrentActivity();
                final Context context = (currentActivity != null) ?
                        currentActivity : GeckoAppShell.getApplicationContext();

                Permissions.from(context)
                           .withPermissions(permissions)
                           .doNotPromptIf(!shouldPrompt || currentActivity == null)
                           .andFallback(new Runnable() {
                               @Override
                               public void run() {
                                   callback.sendSuccess(false);
                               }
                           })
                           .run(new Runnable() {
                               @Override
                               public void run() {
                                   callback.sendSuccess(true);
                               }
                           });

            } else if ("Share:Text".equals(event)) {
                final String text = message.getString("text");
                final String title = message.getString("title", "");
                IntentHelper.openUriExternal(text, "text/plain", "", "",
                                             Intent.ACTION_SEND, title, false);

                // Context: Sharing via chrome list (no explicit session is active)
                Telemetry.sendUIEvent(TelemetryContract.Event.SHARE,
                                      TelemetryContract.Method.LIST, "text");

            } else if ("Snackbar:Show".equals(event)) {
                final Activity currentActivity =
                        GeckoActivityMonitor.getInstance().getCurrentActivity();
                if (currentActivity == null) {
                    if (callback != null) {
                        callback.sendError("No activity");
                    }
                    return;
                }
                SnackbarBuilder.builder(currentActivity)
                        .fromEvent(message)
                        .callback(callback)
                        .buildAndShow();

            } else if ("Distribution:GetDirectories".equals(event)) {
                callback.sendSuccess(Distribution.getDistributionDirectories());

            } else if ("Bookmark:Insert".equals(event)) {
                final Context context = GeckoAppShell.getApplicationContext();
                final BrowserDB db = BrowserDB.from(GeckoThread.getActiveProfile());
                final boolean bookmarkAdded = db.addBookmark(context.getContentResolver(),
                                                             message.getString("title"),
                                                             message.getString("url"));
                final int resId = bookmarkAdded ? R.string.bookmark_added
                                                : R.string.bookmark_already_added;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        final Activity currentActivity =
                                GeckoActivityMonitor.getInstance().getCurrentActivity();
                        if (currentActivity == null) {
                            return;
                        }
                        SnackbarBuilder.builder(currentActivity)
                                .message(resId)
                                .duration(Snackbar.LENGTH_LONG)
                                .buildAndShow();
                    }
                });

            } else if ("Image:SetAs".equals(event)) {
                setImageAs(message.getString("url"));
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

    public static void createShortcut() {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null) {
            createShortcut(selectedTab.getTitle(), selectedTab.getURL());
        }
    }

    // Creates a homescreen shortcut for a web page.
    // This is the entry point from nsIShellService.
    @WrapForJNI(calledFrom = "gecko")
    public static void createShortcut(final String title, final String url) {
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        final String manifestUrl = selectedTab.getManifestUrl();

        if (manifestUrl != null) {
            // If a page has associated manifest, lets install it (PWA A2HS)
            // At this time, this page must be a secure page.
            // Please hide PWA badge UI in front end side.
            // Otherwise we'll throw an exception here.
            final boolean safeForPwa = PwaUtils.shouldAddPwaShortcut(selectedTab);
            if (!safeForPwa) {
                throw new IllegalStateException("This page is not safe for PWA");
            }

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

        ShortcutUtils.createHomescreenIcon(shortcutIntent, aTitle, aURI, aIcon);
    }

    public static void createAppShortcut(final String aTitle, final String aURI,
                                         final String manifestPath, final String manifestUrl,
                                         final Bitmap aIcon) {
        final Intent shortcutIntent = new Intent();
        shortcutIntent.setAction(GeckoApp.ACTION_WEBAPP);
        shortcutIntent.setData(Uri.parse(aURI));
        shortcutIntent.putExtra("MANIFEST_PATH", manifestPath);
        shortcutIntent.putExtra("MANIFEST_URL", manifestUrl);
        shortcutIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                    LauncherActivity.class.getName());
        Telemetry.sendUIEvent(TelemetryContract.Event.ACTION,
                              TelemetryContract.Method.CONTEXT_MENU,
                              "pwa_add_to_launcher");
        ShortcutUtils.createHomescreenIcon(shortcutIntent, aTitle, aURI, aIcon);
    }

    /* package */ static void showSetImageResult(final boolean success, final int message,
                                           final String path) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                final Activity currentActivity =
                        GeckoActivityMonitor.getInstance().getCurrentActivity();
                if (currentActivity == null) {
                    return;
                }

                if (!success) {
                    SnackbarBuilder.builder(currentActivity)
                            .message(message)
                            .duration(Snackbar.LENGTH_LONG)
                            .buildAndShow();
                    return;
                }

                final Intent intent = new Intent(Intent.ACTION_ATTACH_DATA);
                intent.addCategory(Intent.CATEGORY_DEFAULT);
                intent.setData(Uri.parse(path));

                // Removes the image from storage once the chooser activity ends.
                final Context context = GeckoAppShell.getApplicationContext();
                final Intent chooser = Intent.createChooser(intent,
                                                            context.getString(message));
                ActivityResultHandler handler = new ActivityResultHandler() {
                    @Override
                    public void onActivityResult(int resultCode, Intent data) {
                        context.getContentResolver().delete(intent.getData(), null, null);
                    }
                };
                ActivityHandlerHelper.startIntentForActivity(currentActivity, chooser,
                                                             handler);
            }
        });
    }

    // Checks the necessary permissions before attempting to download and
    // set the image as wallpaper.
    private static void setImageAs(final String aSrc) {
        final Activity currentActivity =
                GeckoActivityMonitor.getInstance().getCurrentActivity();
        final Context context = (currentActivity != null) ?
                currentActivity : GeckoAppShell.getApplicationContext();
        Permissions
                .from(context)
                .doNotPromptIf(currentActivity == null)
                .onBackgroundThread()
                .withPermissions(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                .andFallback(new Runnable() {
                    @Override
                    public void run() {
                        showSetImageResult(/* success */ false,
                                           R.string.set_image_path_fail, null);
                    }
                })
                .run(new Runnable() {
                    @Override
                    public void run() {
                        downloadImageForSetImage(aSrc);
                    }
                });
    }


    /**
     * Downloads the image given by <code>aSrc</code> synchronously and
     * then displays the Chooser activity to set the image as wallpaper.
     *
     * @param aSrc The URI to download the image from.
     */
    private static void downloadImageForSetImage(final String aSrc) {
        // Network access from the main thread can cause a StrictMode crash on release builds.
        ThreadUtils.assertOnBackgroundThread();

        final boolean isDataURI = aSrc.startsWith("data:");
        Bitmap image = null;
        InputStream is = null;
        ByteArrayOutputStream os = null;
        try {
            if (isDataURI) {
                int dataStart = aSrc.indexOf(",");
                byte[] buf = Base64.decode(aSrc.substring(dataStart + 1), Base64.DEFAULT);
                image = BitmapUtils.decodeByteArray(buf);
            } else {
                int byteRead;
                byte[] buf = new byte[4192];
                os = new ByteArrayOutputStream();
                URL url = new URL(aSrc);
                is = url.openStream();

                // Cannot read from same stream twice. Also, InputStream from
                // URL does not support reset. So converting to byte array.

                while ((byteRead = is.read(buf)) != -1) {
                    os.write(buf, 0, byteRead);
                }
                byte[] imgBuffer = os.toByteArray();
                image = BitmapUtils.decodeByteArray(imgBuffer);
            }

            if (image != null) {
                // Some devices don't have a DCIM folder and the
                // Media.insertImage call will fail.
                final File dcimDir = Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_PICTURES);
                if (!dcimDir.mkdirs() && !dcimDir.isDirectory()) {
                    showSetImageResult(/* success */ false,
                                       R.string.set_image_path_fail, null);
                    return;
                }

                final Context context = GeckoAppShell.getApplicationContext();
                final String path = MediaStore.Images.Media.insertImage(
                        context.getContentResolver(), image, null, null);
                if (path == null) {
                    showSetImageResult(/* success */ false,
                                       R.string.set_image_path_fail, null);
                    return;
                }
                showSetImageResult(/* success */ true,
                                   R.string.set_image_chooser_title, path);
            } else {
                showSetImageResult(/* success */ false, R.string.set_image_fail, null);
            }
        } catch (final OutOfMemoryError ome) {
            Log.e(LOG_TAG, "Out of Memory when converting to byte array", ome);
        } catch (final IOException ioe) {
            Log.e(LOG_TAG, "I/O Exception while setting wallpaper", ioe);
        } finally {
            if (is != null) {
                try {
                    is.close();
                } catch (final IOException ioe) {
                    Log.w(LOG_TAG, "I/O Exception while closing stream", ioe);
                }
            }
            if (os != null) {
                try {
                    os.close();
                } catch (final IOException ioe) {
                    Log.w(LOG_TAG, "I/O Exception while closing stream", ioe);
                }
            }
        }
    }

    @Override // HapticFeedbackDelegate
    public void performHapticFeedback(final int effect) {
        final Activity currentActivity =
                GeckoActivityMonitor.getInstance().getCurrentActivity();
        if (currentActivity != null) {
            currentActivity.getWindow().getDecorView().performHapticFeedback(effect);
        }
    }
}
