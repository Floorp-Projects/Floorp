/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.updater;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.ContextUtils;
import org.mozilla.gecko.util.GeckoJarReader;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.util.Log;

import java.net.URI;
import java.util.ArrayList;
import java.util.HashMap;

public class UpdateServiceHelper {
    public static final String ACTION_REGISTER_FOR_UPDATES = AppConstants.ANDROID_PACKAGE_NAME + ".REGISTER_FOR_UPDATES";
    public static final String ACTION_UNREGISTER_FOR_UPDATES = AppConstants.ANDROID_PACKAGE_NAME + ".UNREGISTER_FOR_UPDATES";
    public static final String ACTION_CHECK_FOR_UPDATE = AppConstants.ANDROID_PACKAGE_NAME + ".CHECK_FOR_UPDATE";
    public static final String ACTION_CHECK_UPDATE_RESULT = AppConstants.ANDROID_PACKAGE_NAME + ".CHECK_UPDATE_RESULT";
    public static final String ACTION_DOWNLOAD_UPDATE = AppConstants.ANDROID_PACKAGE_NAME + ".DOWNLOAD_UPDATE";
    public static final String ACTION_APPLY_UPDATE = AppConstants.ANDROID_PACKAGE_NAME + ".APPLY_UPDATE";
    public static final String ACTION_CANCEL_DOWNLOAD = AppConstants.ANDROID_PACKAGE_NAME + ".CANCEL_DOWNLOAD";

    // Flags for ACTION_CHECK_FOR_UPDATE
    protected static final int FLAG_FORCE_DOWNLOAD = 1;
    protected static final int FLAG_OVERWRITE_EXISTING = 1 << 1;
    protected static final int FLAG_REINSTALL = 1 << 2;
    protected static final int FLAG_RETRY = 1 << 3;

    // Name of the Intent extra for the autodownload policy, used with ACTION_REGISTER_FOR_UPDATES
    protected static final String EXTRA_AUTODOWNLOAD_NAME = "autodownload";

    // Name of the Intent extra that holds the flags for ACTION_CHECK_FOR_UPDATE
    protected static final String EXTRA_UPDATE_FLAGS_NAME = "updateFlags";

    // Name of the Intent extra that holds the APK path, used with ACTION_APPLY_UPDATE
    protected static final String EXTRA_PACKAGE_PATH_NAME = "packagePath";

    // Name of the Intent extra for the update URL, used with ACTION_REGISTER_FOR_UPDATES
    protected static final String EXTRA_UPDATE_URL_NAME = "updateUrl";

    private static final String LOGTAG = "UpdateServiceHelper";
    private static final String DEFAULT_UPDATE_LOCALE = "en-US";

    // So that updates can be disabled by tests.
    private static volatile boolean isEnabled = true;

    private enum Pref {
        AUTO_DOWNLOAD_POLICY("app.update.autodownload"),
        UPDATE_URL("app.update.url.android");

        public final String name;

        private Pref(String name) {
            this.name = name;
        }

        public final static String[] names;

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
            Log.i(LOGTAG, "Failed to read update locale file, falling back to " + locale);
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

    public static void setAutoDownloadPolicy(Context context, UpdateService.AutoDownloadPolicy policy) {
        registerForUpdates(context, policy, null);
    }

    public static void checkForUpdate(Context context) {
        if (context == null) {
            return;
        }

        context.startService(createIntent(context, ACTION_CHECK_FOR_UPDATE));
    }

    public static void downloadUpdate(Context context) {
        if (context == null) {
            return;
        }

        context.startService(createIntent(context, ACTION_DOWNLOAD_UPDATE));
    }

    public static void applyUpdate(Context context) {
        if (context == null) {
            return;
        }

        context.startService(createIntent(context, ACTION_APPLY_UPDATE));
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
                    UpdateService.AutoDownloadPolicy.get(
                        (String) prefs.get(Pref.AUTO_DOWNLOAD_POLICY.toString())),
                      (String) prefs.get(Pref.UPDATE_URL.toString()));
            }
        });
    }

    public static void registerForUpdates(Context context, UpdateService.AutoDownloadPolicy policy, String url) {
        if (!isUpdaterEnabled(context)) {
             return;
        }

        Intent intent = createIntent(context, ACTION_REGISTER_FOR_UPDATES);

        if (policy != null) {
            intent.putExtra(EXTRA_AUTODOWNLOAD_NAME, policy.value);
        }

        if (url != null) {
            intent.putExtra(EXTRA_UPDATE_URL_NAME, url);
        }

        context.startService(intent);
    }

    private static Intent createIntent(Context context, String action) {
        return new Intent(action, null, context, UpdateService.class);
    }
}
