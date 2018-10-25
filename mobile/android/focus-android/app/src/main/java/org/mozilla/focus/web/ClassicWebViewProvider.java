/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.StrictMode;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebView;

import org.jetbrains.annotations.NotNull;
import org.mozilla.focus.R;
import org.mozilla.focus.utils.Settings;
import org.mozilla.focus.webview.SystemWebView;
import org.mozilla.focus.webview.TrackingProtectionWebViewClient;

/**
 * WebViewProvider for creating a WebView based IWebView implementation.
 */
public class ClassicWebViewProvider implements IWebViewProvider {
    /**
     * Preload webview data. This allows the webview implementation to load resources and other data
     * it might need, in advance of intialising the view (at which time we are probably wanting to
     * show a website immediately).
     */
    public void preload(@NonNull final Context context) {
        TrackingProtectionWebViewClient.triggerPreload(context);
    }

    public void performCleanup(@NonNull final Context context) {
        SystemWebView.Companion.deleteContentFromKnownLocations(context);
    }

    /**
     * A cleanup that should occur when a new browser session starts. This might be able to be merged with
     * {@link #performCleanup(Context)}, but I didn't want to do it now to avoid unforeseen side effects. We can do this
     * when we rethink our erase strategy: #1472.
     *
     * This function must be called before WebView.loadUrl to avoid erasing current session data.
     */
    public void performNewBrowserSessionCleanup() {
        // If the app is closed in certain ways, WebView.cleanup will not get called and we don't clear cookies.
        CookieManager.getInstance().removeAllCookies(null);

        // We run this on the main thread to guarantee it occurs before loadUrl so we don't erase current session data.
        final StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();

        // When left open on erase, some pages, like the google search results, will asynchronously write LocalStorage
        // files to disk after we erase them. To work-around this, we delete this data again when starting a new browser session.
        WebStorage.getInstance().deleteAllData();

        StrictMode.setThreadPolicy(oldPolicy);
    }

    @NonNull
    public View create(@NonNull Context context, AttributeSet attrs) {
        final SystemWebView webkitView = new SystemWebView(context, attrs);
        final WebSettings settings = webkitView.getSettings();

        setupView(webkitView);
        configureDefaultSettings(context, settings);
        applyAppSettings(context, settings, webkitView);

        return webkitView;
    }

    private static void setupView(WebView webView) {
        webView.setVerticalScrollBarEnabled(true);
        webView.setHorizontalScrollBarEnabled(true);
    }

    @SuppressLint("SetJavaScriptEnabled") // We explicitly want to enable JavaScript
    private void configureDefaultSettings(Context context, WebSettings settings) {
        settings.setJavaScriptEnabled(true);

        // Needs to be enabled to display some HTML5 sites that use local storage
        settings.setDomStorageEnabled(true);

        // Enabling built in zooming shows the controls by default
        settings.setBuiltInZoomControls(true);

        // So we hide the controls after enabling zooming
        settings.setDisplayZoomControls(false);

        // To respect the html viewport:
        settings.setLoadWithOverviewMode(true);

        // Also increase text size to fill the viewport (this mirrors the behaviour of Firefox,
        // Chrome does this in the current Chrome Dev, but not Chrome release).
        settings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.TEXT_AUTOSIZING);

        // Disable access to arbitrary local files by webpages - assets can still be loaded
        // via file:///android_asset/res, so at least error page images won't be blocked.
        settings.setAllowFileAccess(false);
        settings.setAllowFileAccessFromFileURLs(false);
        settings.setAllowUniversalAccessFromFileURLs(false);

        final String appName = context.getResources().getString(R.string.useragent_appname);
        settings.setUserAgentString(buildUserAgentString(context, settings, appName));

        // Right now I do not know why we should allow loading content from a content provider
        settings.setAllowContentAccess(false);

        // The default for those settings should be "false" - But we want to be explicit.
        settings.setAppCacheEnabled(false);
        settings.setDatabaseEnabled(false);
        settings.setJavaScriptCanOpenWindowsAutomatically(false);

        // We do not implement the callbacks - So let's disable it.
        settings.setGeolocationEnabled(false);

        // We do not want to save any data...
        settings.setSaveFormData(false);
        //noinspection deprecation - This method is deprecated but let's call it in case WebView implementations still obey it.
        settings.setSavePassword(false);
    }

    @Override
    public void applyAppSettings(@NotNull Context context, @NotNull WebSettings webSettings, @NotNull SystemWebView systemWebView) {

        // Clear the cache so trackers previously loaded are removed
        systemWebView.clearCache(true);

        // We could consider calling setLoadsImagesAutomatically() here too (This will block images not loaded over the network too)
        webSettings.setBlockNetworkImage(Settings.getInstance(context).shouldBlockImages());
        webSettings.setJavaScriptEnabled(!Settings.getInstance(context).shouldBlockJavaScript());
        CookieManager.getInstance().setAcceptThirdPartyCookies(systemWebView, !Settings.getInstance
                (context).shouldBlockThirdPartyCookies());
        CookieManager.getInstance().setAcceptCookie(!Settings.getInstance(context)
                .shouldBlockCookies());
    }

    @Override
    @SuppressLint("SetJavaScriptEnabled") // We explicitly want to enable JavaScript
    public void disableBlocking(@NotNull WebSettings webSettings, @NotNull SystemWebView systemWebView) {
        webSettings.setBlockNetworkImage(false);
        webSettings.setJavaScriptEnabled(true);
        CookieManager.getInstance().setAcceptThirdPartyCookies(systemWebView, true);
        CookieManager.getInstance().setAcceptCookie(true);
    }

    public void requestDesktopSite(@NonNull WebSettings settings) {
        settings.setUserAgentString(toggleDesktopUA(settings, true));
        settings.setUseWideViewPort(true);
    }

    public void requestMobileSite(@NonNull Context context, @NonNull WebSettings settings) {
        settings.setUserAgentString(toggleDesktopUA(settings, false));
        settings.setUseWideViewPort(false);
    }

    /**
     * Build the browser specific portion of the UA String, based on the webview's existing UA String.
     */
    @NonNull
    public String getUABrowserString(@NonNull final String existingUAString, @NonNull final String focusToken) {
        // Use the default WebView agent string here for everything after the platform, but insert
        // Focus in front of Chrome.
        // E.g. a default webview UA string might be:
        // Mozilla/5.0 (Linux; Android 7.1.1; Pixel XL Build/NOF26V; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/56.0.2924.87 Mobile Safari/537.36
        // And we reuse everything from AppleWebKit onwards, except for adding Focus.
        int start = existingUAString.indexOf("AppleWebKit");
        if (start == -1) {
            // I don't know if any devices don't include AppleWebKit, but given the diversity of Android
            // devices we should have a fallback: we search for the end of the platform String, and
            // treat the next token as the start:
            start = existingUAString.indexOf(")") + 2;

            // If this was located at the very end, then there's nothing we can do, so let's just
            // return focus:
            if (start >= existingUAString.length()) {
                return focusToken;
            }
        }

        final String[] tokens = existingUAString.substring(start).split(" ");

        for (int i = 0; i < tokens.length; i++) {
            if (tokens[i].startsWith("Chrome")) {
                tokens[i] = focusToken + " " + tokens[i];

                return TextUtils.join(" ", tokens);
            }
        }

        // If we didn't find a chrome token, we just append the focus token at the end:
        return TextUtils.join(" ", tokens) + " " + focusToken;
    }

    private String buildUserAgentString(final Context context, final WebSettings settings, final String appName) {
        final StringBuilder uaBuilder = new StringBuilder();

        uaBuilder.append("Mozilla/5.0");

        // WebView by default includes "; wv" as part of the platform string, but we're a full browser
        // so we shouldn't include that.
        // Most webview based browsers (and chrome), include the device name AND build ID, e.g.
        // "Pixel XL Build/NOF26V", that seems unnecessary (and not great from a privacy perspective),
        // so we skip that too.
        uaBuilder.append(" (Linux; Android ").append(Build.VERSION.RELEASE).append(") ");

        final String existingWebViewUA = settings.getUserAgentString();

        final String appVersion;
        try {
            appVersion = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException e) {
            // This should be impossible - we should always be able to get information about ourselves:
            throw new IllegalStateException("Unable find package details for Focus", e);
        }

        final String focusToken = appName + "/" + appVersion;
        uaBuilder.append(getUABrowserString(existingWebViewUA, focusToken));

        return uaBuilder.toString();
    }

    private static String toggleDesktopUA(final WebSettings settings, final boolean requestDesktop) {
        final String existingUAString = settings.getUserAgentString();
        if (requestDesktop) {
            return existingUAString.replace("Mobile", "eliboM").replace("Android", "diordnA");
        } else {
            return existingUAString.replace("eliboM", "Mobile").replace("diordnA", "Android");
        }
    }
}
