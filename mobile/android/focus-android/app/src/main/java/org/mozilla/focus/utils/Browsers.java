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

    private final Map<String, ActivityInfo> browsers;
    private ActivityInfo defaultBrowser;

    public Browsers(Context context, String url) {
        final PackageManager packageManager = context.getPackageManager();

        final Map<String, ActivityInfo> browsers = resolveBrowsers(packageManager, url);
        if (browsers.size() <= 1) {
            // If there's a default browser set then modern Android systems won't return other options
            // anymore when using queryIntentActivities(). That's annoying and our only option is
            // to go through a list of known browsers and see if anyone of them is installed and
            // wants to handle our URL.
            findKnownBrowsers(packageManager, browsers, url);
        }

        this.browsers = browsers;
        this.defaultBrowser = findDefault(packageManager, url);
    }

    private Map<String, ActivityInfo> resolveBrowsers(PackageManager packageManager, String url) {
        final Map<String, ActivityInfo> browsers = new HashMap<>();

        final Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        final List<ResolveInfo> infos = packageManager.queryIntentActivities(intent, 0);

        for (ResolveInfo info : infos) {
            browsers.put(info.activityInfo.packageName, info.activityInfo);
        }

        return browsers;
    }

    private void findKnownBrowsers(PackageManager packageManager, Map<String, ActivityInfo> browsers, String url) {
        for (KnownBrowser browser : KnownBrowser.values()) {
            if (browsers.containsKey(browser.packageName)) {
                continue;
            }

            final Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(url));
            intent.setPackage(browser.packageName);

            final ResolveInfo info = packageManager.resolveActivity(intent, 0);
            if (info == null || info.activityInfo == null) {
                continue;
            }

            browsers.put(info.activityInfo.packageName, info.activityInfo);
        }
    }

    private ActivityInfo findDefault(PackageManager packageManager, String url) {
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));

        final ResolveInfo resolveInfo = packageManager.resolveActivity(intent, 0);
        if (resolveInfo == null || resolveInfo.activityInfo == null) {
            return null;
        }

        if ("android".equals(resolveInfo.activityInfo.packageName)) {
            // If there's actually no default then Android might return a system activity that we
            // are not interested in.
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
                && !defaultBrowser.packageName.equals(context.getPackageName());
    }

    public ActivityInfo getDefaultBrowser() {
        return defaultBrowser;
    }

    /**
     * Does this user have browsers installed that are not Focus, Firefox or the default browser?
     */
    public boolean hasMultipleThirdPartyBrowsers(Context context) {
        if (browsers.size() > 2) {
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

    public ActivityInfo[] getInstalledBrowsers() {
        final Collection<ActivityInfo> collection = browsers.values();

        return collection.toArray(new ActivityInfo[collection.size()]);
    }
}
