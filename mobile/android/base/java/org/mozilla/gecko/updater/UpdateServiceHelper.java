/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.v4.app.JobIntentService;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.JobIdsConstants;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.util.ContextUtils;
import org.mozilla.gecko.util.GeckoJarReader;

import java.net.URI;
import java.util.ArrayList;
import java.util.HashMap;

public class UpdateServiceHelper {
    // Following can be used to start a specific UpdaterService
    public static final String ACTION_REGISTER_FOR_UPDATES = AppConstants.ANDROID_PACKAGE_NAME + ".REGISTER_FOR_UPDATES";
    public static final String ACTION_CHECK_FOR_UPDATE = AppConstants.ANDROID_PACKAGE_NAME + ".CHECK_FOR_UPDATE";
    public static final String ACTION_DOWNLOAD_UPDATE = AppConstants.ANDROID_PACKAGE_NAME + ".DOWNLOAD_UPDATE";
    public static final String ACTION_APPLY_UPDATE = AppConstants.ANDROID_PACKAGE_NAME + ".APPLY_UPDATE";

    // Used to inform Gecko about current status
    public static final String ACTION_CHECK_UPDATE_RESULT = AppConstants.ANDROID_PACKAGE_NAME + ".CHECK_UPDATE_RESULT"; //
    // Used to change running service state
    public static final String ACTION_CANCEL_DOWNLOAD = AppConstants.ANDROID_PACKAGE_NAME + ".CANCEL_DOWNLOAD";     //

    // Update intervals
    private static final int INTERVAL_LONG = 1000 * 60 * 60 * 24; // 1 day in milliseconds
    private static final int INTERVAL_SHORT = 1000 * 60 * 60 * 4; // 4 hours in milliseconds
    private static final int INTERVAL_RETRY = 1000 * 60 * 60 * 1; // 1 hour in milliseconds

    // The number of bytes to read when working with the updated package
    static final int BUFSIZE = 8192;

    // All the notifications posted by the update services will use the same id
    static final int NOTIFICATION_ID = 0x3e40ddbd;

    // Name of the Intent extra for the autodownload policy, used with ACTION_REGISTER_FOR_UPDATES
    protected static final String EXTRA_AUTODOWNLOAD_NAME = "autodownload";

    // Name of the Intent extra that holds the flags for ACTION_CHECK_FOR_UPDATE
    protected static final String EXTRA_UPDATE_FLAGS_NAME = "updateFlags";

    // Name of the Intent extra that holds the APK path, used with ACTION_APPLY_UPDATE
    protected static final String EXTRA_PACKAGE_PATH_NAME = "packagePath";

    // Name of the Intent extra for the update URL, used with ACTION_REGISTER_FOR_UPDATES
    protected static final String EXTRA_UPDATE_URL_NAME = "updateUrl";

    private static final String LOGTAG = "GeckoUpdatesHelper";
    private static final boolean DEBUG = false;
    private static final String DEFAULT_UPDATE_LOCALE = "en-US";

    // So that updates can be disabled by tests.
    private static volatile boolean isEnabled = true;

    static final class UpdateInfo {
        public URI uri;
        public String buildID;
        public String hashFunction;
        public String hashValue;
        public int size;

        private boolean isNonEmpty(String s) {
            return s != null && s.length() > 0;
        }

        public boolean isValid() {
            return uri != null && isNonEmpty(buildID) &&
                    isNonEmpty(hashFunction) && isNonEmpty(hashValue) && size > 0;
        }

        @Override
        public String toString() {
            return "uri = " + uri + ", buildID = " + buildID + ", hashFunction = " + hashFunction + ", hashValue = " + hashValue + ", size = " + size;
        }
    }

    public enum AutoDownloadPolicy {
        NONE(-1),
        WIFI(0),
        DISABLED(1),
        ENABLED(2);

        public final int value;

        AutoDownloadPolicy(int value) {
            this.value = value;
        }

        private final static AutoDownloadPolicy[] sValues = AutoDownloadPolicy.values();

        public static AutoDownloadPolicy get(int value) {
            for (AutoDownloadPolicy id: sValues) {
                if (id.value == value) {
                    return id;
                }
            }
            return NONE;
        }

        public static AutoDownloadPolicy get(String name) {
            for (AutoDownloadPolicy id: sValues) {
                if (name.equalsIgnoreCase(id.toString())) {
                    return id;
                }
            }
            return NONE;
        }
    }

    enum CheckUpdateResult {
        // Keep these in sync with mobile/android/chrome/content/about.xhtml
        NOT_AVAILABLE,
        AVAILABLE,
        DOWNLOADING,
        DOWNLOADED
    }

    private enum Pref {
        AUTO_DOWNLOAD_POLICY("app.update.autodownload"),
        UPDATE_URL("app.update.url.android");

        final String name;

        Pref(String name) {
            this.name = name;
        }

        final static String[] names;

        @Override
        public String toString() {
            return this.name;
        }

        static {
            ArrayList<String> nameList = new ArrayList<String>();

            for (Pref id: Pref.values()) {
                nameList.add(id.toString());
            }

            names = nameList.toArray(new String[0]);
        }
    }

    @RobocopTarget
    public static void setEnabled(final boolean enabled) {
        isEnabled = enabled;
    }

    public static URI expandUpdateURI(Context context, String updateUri, boolean force) {
        if (updateUri == null) {
            return null;
        }

        PackageManager pm = context.getPackageManager();

        String pkgSpecial = AppConstants.MOZ_PKG_SPECIAL != null ?
                            "-" + AppConstants.MOZ_PKG_SPECIAL :
                            "";
        String locale = DEFAULT_UPDATE_LOCALE;

        try {
            ApplicationInfo info = pm.getApplicationInfo(AppConstants.ANDROID_PACKAGE_NAME, 0);
            String updateLocaleUrl = "jar:jar:file://" + info.sourceDir + "!/" + AppConstants.OMNIJAR_NAME + "!/update.locale";

            final String jarLocale = GeckoJarReader.getText(context, updateLocaleUrl);
            if (jarLocale != null) {
                locale = jarLocale.trim();
            }
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
            // Shouldn't really be possible, but fallback to default locale
            if (DEBUG) {
                Log.i(LOGTAG, "Failed to read update locale file, falling back to " + locale);
            }
        }

        String url = updateUri.replace("%PRODUCT%", AppConstants.MOZ_APP_BASENAME)
            .replace("%VERSION%", AppConstants.MOZ_APP_VERSION)
            .replace("%BUILD_ID%", force ? "0" : AppConstants.MOZ_APP_BUILDID)
            .replace("%BUILD_TARGET%", "Android_" + AppConstants.MOZ_APP_ABI + pkgSpecial)
            .replace("%LOCALE%", locale)
            .replace("%CHANNEL%", AppConstants.MOZ_UPDATE_CHANNEL)
            .replace("%OS_VERSION%", Build.VERSION.RELEASE)
            .replace("%DISTRIBUTION%", "default")
            .replace("%DISTRIBUTION_VERSION%", "default")
            .replace("%MOZ_VERSION%", AppConstants.MOZILLA_VERSION);

        try {
            return new URI(url);
        } catch (java.net.URISyntaxException e) {
            Log.e(LOGTAG, "Failed to create update url: ", e);
            return null;
        }
    }

    public static boolean isUpdaterEnabled(final Context context) {
        return AppConstants.MOZ_UPDATER && isEnabled && !ContextUtils.isInstalledFromGooglePlay(context);
    }

    public static void setUpdateUrl(Context context, String url) {
        registerForUpdates(context, null, url);
    }

    public static void setAutoDownloadPolicy(Context context, AutoDownloadPolicy policy) {
        registerForUpdates(context, policy, null);
    }

    public static void checkForUpdate(Context context) {
        if (context == null) {
            return;
        }

        enqueueUpdateCheck(context, new Intent());
    }

    public static void downloadUpdate(Context context) {
        if (context == null) {
            return;
        }

        enqueueUpdateDownload(context, new Intent());
    }

    public static void applyUpdate(Context context) {
        if (context == null) {
            return;
        }

        enqueueUpdateApply(context, new Intent());
    }

    public static void registerForUpdates(final Context context) {
        if (!isUpdaterEnabled(context)) {
             return;
        }

        final HashMap<String, Object> prefs = new HashMap<String, Object>();

        PrefsHelper.getPrefs(Pref.names, new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, String value) {
                prefs.put(pref, value);
            }

            @Override public void finish() {
                UpdateServiceHelper.registerForUpdates(context,
                    AutoDownloadPolicy.get(
                        (String) prefs.get(Pref.AUTO_DOWNLOAD_POLICY.toString())),
                      (String) prefs.get(Pref.UPDATE_URL.toString()));
            }
        });
    }

    private static void registerForUpdates(Context context, AutoDownloadPolicy policy, String url) {
        if (!isUpdaterEnabled(context)) {
             return;
        }

        Intent intent = new Intent(ACTION_REGISTER_FOR_UPDATES);

        if (policy != null) {
            intent.putExtra(EXTRA_AUTODOWNLOAD_NAME, policy.value);
        }

        if (url != null) {
            intent.putExtra(EXTRA_UPDATE_URL_NAME, url);
        }

        enqueueUpdateRegister(context, intent);
    }

    static int getUpdateInterval(boolean isRetry) {
        int interval;
        if (isRetry) {
            interval = INTERVAL_RETRY;
        } else if (!AppConstants.RELEASE_OR_BETA) {
            interval = INTERVAL_SHORT;
        } else {
            interval = INTERVAL_LONG;
        }

        return interval;
    }

    public static void enqueueUpdateApply(@NonNull final Context context, @NonNull final Intent workIntent) {
        JobIntentService.enqueueWork(context, UpdatesApplyService.class,
                JobIdsConstants.getIdForUpdatesApplyJob(), workIntent);
    }

    public static void enqueueUpdateCheck(@NonNull final Context context, @NonNull final Intent workIntent) {
        JobIntentService.enqueueWork(context, UpdatesCheckService.class,
                JobIdsConstants.getIdForUpdatesCheckJob(), workIntent);
    }

    public static void enqueueUpdateDownload(@NonNull final Context context, @NonNull final Intent workIntent) {
        JobIntentService.enqueueWork(context, UpdatesDownloadService.class,
                JobIdsConstants.getIdForUpdatesDownloadJob(), workIntent);
    }

    public static void enqueueUpdateRegister(@NonNull final Context context, @NonNull final Intent workIntent) {
        JobIntentService.enqueueWork(context, UpdatesRegisterService.class,
                JobIdsConstants.getIdForUpdatesRegisterJob(), workIntent);
    }
}
