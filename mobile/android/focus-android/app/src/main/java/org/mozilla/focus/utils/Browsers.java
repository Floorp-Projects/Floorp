/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Helpful tools for dealing with other browsers.
 */
public class Browsers {
    public enum KnownBrowser {
        FIREFOX("org.mozilla.firefox"),

        FIREFOX_BETA("org.mozilla.firefox_beta"),
        FIREFOX_AURORA("org.mozilla.fennec_aurora"),
        FIREFOX_NIGHTLY("org.mozilla.fennec"),
        FIREFOX_ROCKET("org.mozilla.rocket"),

        CHROME("com.android.chrome"),
        CHROME_BETA("com.chrome.beta"),
        CHROME_DEV("com.chrome.dev"),
        CHROME_CANARY("com.chrome.canary"),

        OPERA("com.opera.browser"),
        OPERA_BETA("com.opera.browser.beta"),
        OPERA_MINI("com.opera.mini.native"),
        OPERA_MINI_BETA("com.opera.mini.native.beta"),

        UC_BROWSER("com.UCMobile.intl"),
        UC_BROWSER_MINI("com.uc.browser.en"),

        ANDROID_STOCK_BROWSER("com.android.browser"),

        SAMSUNG_INTERNET("com.sec.android.app.sbrowser"),
        DOLPHIN_BROWSER("mobi.mgeek.TunnyBrowser"),
        BRAVE_BROWSER("com.brave.browser"),
        LINK_BUBBLE("com.linkbubble.playstore"),
        ADBLOCK_BROWSER("org.adblockplus.browser"),
        CHROMER("arun.com.chromer"),
        FLYNX("com.flynx"),
        GHOSTERY_BROWSER("com.ghostery.android.ghostery");

        public final String packageName;

        KnownBrowser(String packageName) {
            this.packageName = packageName;
        }
    }

    // Sample URL handled by traditional web browsers. Used to find installed (basic) web browsers.
    public static final String TRADITIONAL_BROWSER_URL = "http://www.mozilla.org";

    private final Map<String, ActivityInfo> browsers;
    private final ActivityInfo defaultBrowser;
    // This will contain installed firefox branded browser ordered by priority from Firefox,
    // Firefox_Beta, Firefox Aurora and Firefox_Nightly. If multiple firefox branded browser is
    // installed then higher priority one will be stored here
    private ActivityInfo firefoxBrandedBrowser;

    public Browsers(Context context, @NonNull String url) {
        final PackageManager packageManager = context.getPackageManager();

        final Uri uri = Uri.parse(url);

        final Map<String, ActivityInfo> browsers = resolveBrowsers(context, packageManager, uri);

        // If there's a default browser set then modern Android systems won't return other browsers
        // anymore when using queryIntentActivities(). That's annoying and our only option is
        // to go through a list of known browsers and see if anyone of them is installed and
        // wants to handle our URL.
        findKnownBrowsers(packageManager, browsers, uri);

        this.browsers = browsers;
        this.defaultBrowser = findDefault(context, packageManager, uri);
        this.firefoxBrandedBrowser = findFirefoxBrandedBrowser();
    }

    private ActivityInfo findFirefoxBrandedBrowser() {
        if (browsers.containsKey(KnownBrowser.FIREFOX.packageName)) {
            return browsers.get(KnownBrowser.FIREFOX.packageName);
        } else if (browsers.containsKey(KnownBrowser.FIREFOX_BETA.packageName)) {
            return browsers.get(KnownBrowser.FIREFOX_BETA.packageName);
        } else if (browsers.containsKey(KnownBrowser.FIREFOX_AURORA.packageName)) {
            return browsers.get(KnownBrowser.FIREFOX_AURORA.packageName);
        } else if (browsers.containsKey(KnownBrowser.FIREFOX_NIGHTLY.packageName)) {
            return browsers.get(KnownBrowser.FIREFOX_NIGHTLY.packageName);
        }
        return null;
    }

    private Map<String, ActivityInfo> resolveBrowsers(Context context, PackageManager packageManager, @NonNull Uri uri) {
        final Map<String, ActivityInfo> browsers = new HashMap<>();

        final Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(uri);

        final List<ResolveInfo> infos = packageManager.queryIntentActivities(intent, 0);

        for (ResolveInfo info : infos) {
            if (!context.getPackageName().equals(info.activityInfo.packageName)) {
                browsers.put(info.activityInfo.packageName, info.activityInfo);
            }
        }

        return browsers;
    }

    private void findKnownBrowsers(PackageManager packageManager, Map<String, ActivityInfo> browsers, @NonNull Uri uri) {
        for (final KnownBrowser browser : KnownBrowser.values()) {
            if (browsers.containsKey(browser.packageName)) {
                continue;
            }

            // resolveActivity() can be slow if the package isn't installed (e.g. 200ms on an N6 with a bad WiFi connection).
            // Hence we query if the package is installed first, and only call resolveActivity for installed packages.
            // getPackageInfo() is fast regardless of a package being installed
            try {
                // We don't need the result, we only need to detect when the package doesn't exist
                packageManager.getPackageInfo(browser.packageName, 0);
            } catch (PackageManager.NameNotFoundException e) {
                continue;
            }

            final Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(uri);
            intent.setPackage(browser.packageName);

            final ResolveInfo info = packageManager.resolveActivity(intent, 0);
            if (info == null || info.activityInfo == null) {
                continue;
            }

            if (!info.activityInfo.exported) {
                continue;
            }

            browsers.put(info.activityInfo.packageName, info.activityInfo);
        }
    }

    private ActivityInfo findDefault(Context context, PackageManager packageManager, @NonNull Uri uri) {
        final Intent intent = new Intent(Intent.ACTION_VIEW, uri);

        final ResolveInfo resolveInfo = packageManager.resolveActivity(intent, 0);
        if (resolveInfo == null || resolveInfo.activityInfo == null) {
            return null;
        }

        if (!resolveInfo.activityInfo.exported) {
            // We are not allowed to launch this activity.
            return null;
        }

        if (!browsers.containsKey(resolveInfo.activityInfo.packageName)
                && !resolveInfo.activityInfo.packageName.equals(context.getPackageName())) {
            // This default browser wasn't returned when we asked for *all* browsers. It's likely
            // that this is actually the resolver activity (aka intent chooser). Let's ignore it.
            return null;
        }

        return resolveInfo.activityInfo;
    }

    /**
     * Does this user have a default browser that is not Firefox (release) or Focus (this build).
     */
    public boolean hasThirdPartyDefaultBrowser(Context context) {
        return defaultBrowser != null
                && !defaultBrowser.packageName.equals(KnownBrowser.FIREFOX.packageName)
                && !(firefoxBrandedBrowser != null && defaultBrowser.packageName.equals(firefoxBrandedBrowser.packageName))
                && !defaultBrowser.packageName.equals(context.getPackageName());
    }

    /**
     * Is (regular) the default browser of the user?
     */
    public boolean isFirefoxDefaultBrowser() {
        if (defaultBrowser == null) {
            return false;
        }

        return defaultBrowser.packageName.equals(KnownBrowser.FIREFOX.packageName)
                || defaultBrowser.packageName.equals(KnownBrowser.FIREFOX_BETA.packageName)
                || defaultBrowser.packageName.equals(KnownBrowser.FIREFOX_AURORA.packageName)
                || defaultBrowser.packageName.equals(KnownBrowser.FIREFOX_NIGHTLY.packageName);
    }

    public @Nullable ActivityInfo getDefaultBrowser() {
        return defaultBrowser;
    }

    /**
     * Does this user have browsers installed that are not Focus, Firefox or the default browser?
     */
    public boolean hasMultipleThirdPartyBrowsers(Context context) {
        if (browsers.size() > 1) {
            // There are more than us and Firefox.
            return true;
        }

        for (ActivityInfo info : browsers.values()) {
            if (info != defaultBrowser
                    && !info.packageName.equals(KnownBrowser.FIREFOX.packageName)
                    && !info.packageName.equals(context.getPackageName())) {
                // There's at least one browser that is not Focus or Firefox and also not the
                // default browser.
                return true;
            }
        }

        return false;
    }

    public boolean isInstalled(KnownBrowser browser) {
        return browsers.containsKey(browser.packageName);
    }

    /**
     * Is *this* application the default browser?
     */
    public boolean isDefaultBrowser(Context context) {
        return defaultBrowser != null && context.getPackageName().equals(defaultBrowser.packageName);

    }

    public ActivityInfo[] getInstalledBrowsers() {
        final Collection<ActivityInfo> collection = browsers.values();

        return collection.toArray(new ActivityInfo[collection.size()]);
    }

    public boolean hasFirefoxBrandedBrowserInstalled() {
        return firefoxBrandedBrowser != null;
    }

    public ActivityInfo getFirefoxBrandedBrowser() {
        return firefoxBrandedBrowser;
    }
}
