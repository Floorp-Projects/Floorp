/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.webkit.CookieManager;
import android.webkit.DownloadListener;
import android.webkit.WebBackForwardList;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewDatabase;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.R;
import org.mozilla.focus.utils.FileUtils;
import org.mozilla.focus.utils.Settings;
import org.mozilla.focus.utils.ThreadUtils;
import org.mozilla.focus.webkit.NestedWebView;
import org.mozilla.focus.webkit.TrackingProtectionWebViewClient;

/**
 * WebViewProvider for creating a WebKit based IWebVIew implementation.
 */
public class WebViewProvider {
    private static final String KEY_CURRENTURL = "currenturl";

    /**
     * Preload webview data. This allows the webview implementation to load resources and other data
     * it might need, in advance of intialising the view (at which time we are probably wanting to
     * show a website immediately).
     */
    public static void preload(final Context context) {
        TrackingProtectionWebViewClient.triggerPreload(context);
    }

    public static void performCleanup(final Context context) {
        // Although most of the (cookie/cache/etc) data isn't instance specific, the cleanup methods
        // aren't static, hence we need to grab a WebView instance:
        final WebkitView webView = new WebkitView(context, null);
        // Calls our data cleanup code:
        webView.cleanup();
        // Calls the system webview cleanup/deinit code:
        webView.destroy();
    }

    public static View create(Context context, AttributeSet attrs) {
        final WebkitView webkitView = new WebkitView(context, attrs);
        final WebSettings settings = webkitView.getSettings();

        setupView(webkitView);
        configureDefaultSettings(context, settings);
        applyAppSettings(context, settings);

        return webkitView;
    }

    private static void setupView(WebView webView) {
        webView.setVerticalScrollBarEnabled(true);
        webView.setHorizontalScrollBarEnabled(true);
    }

    @SuppressLint("SetJavaScriptEnabled") // We explicitly want to enable JavaScript
    private static void configureDefaultSettings(Context context, WebSettings settings) {
        settings.setJavaScriptEnabled(true);

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

        settings.setUserAgentString(buildUserAgentString(context, settings));

        // Right now I do not know why we should allow loading content from a content provider
        settings.setAllowContentAccess(false);

        // The default for those settings should be "false" - But we want to be explicit.
        settings.setAppCacheEnabled(false);
        settings.setDatabaseEnabled(false);
        settings.setDomStorageEnabled(false);
        settings.setJavaScriptCanOpenWindowsAutomatically(false);

        // We do not implement the callbacks - So let's disable it.
        settings.setGeolocationEnabled(false);

        // We do not want to save any data...
        settings.setSaveFormData(false);
        //noinspection deprecation - This method is deprecated but let's call it in case WebView implementations still obey it.
        settings.setSavePassword(false);
    }

    private static void applyAppSettings(Context context, WebSettings settings) {
        final Settings appSettings = new Settings(context);

        // We could consider calling setLoadsImagesAutomatically() here too (This will block images not loaded over the network too)
        settings.setBlockNetworkImage(appSettings.shouldBlockImages());
    }

    /**
     * Build the browser specific portion of the UA String, based on the webview's existing UA String.
     */
    @VisibleForTesting static String getUABrowserString(final String existingUAString, final String focusToken) {
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
        return TextUtils.join(" ", tokens) + focusToken;
    }

    private static String buildUserAgentString(final Context context, final WebSettings settings) {
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

        final String focusToken = context.getResources().getString(R.string.useragent_appname) + "/" + appVersion;
        uaBuilder.append(getUABrowserString(existingWebViewUA, focusToken));

        return uaBuilder.toString();
    }

    private static class LinkHandler implements View.OnLongClickListener, View.OnTouchListener {
        private final WebView webView;
        private @Nullable IWebView.Callback callback = null;

        public LinkHandler(final WebView webView) {
            this.webView = webView;
        }

        public void setCallback(final @Nullable IWebView.Callback callback) {
            this.callback = callback;
        }

        private float lastX;
        private float lastY;

        @Override
        public boolean onTouch(View v, MotionEvent event) {
            lastX = event.getX();
            lastY = event.getY();
            return false;
        }

        @Override
        public boolean onLongClick(View v) {
            if (callback == null) {
                return false;
            }

            final WebView.HitTestResult hitTestResult = webView.getHitTestResult();

            switch (hitTestResult.getType()) {
                case WebView.HitTestResult.SRC_ANCHOR_TYPE:
                    final String linkURL = hitTestResult.getExtra();
                    callback.onLongPress(new IWebView.ClickTarget(true, linkURL, false, null), lastX, lastY);
                    return true;

                case WebView.HitTestResult.SRC_IMAGE_ANCHOR_TYPE:
                    // hitTestResult.getExtra() contains only the image URL, and not the link
                    // URL. Internally, WebView's HitTestData contains both, but they only
                    // make it available via requestFocusNodeHref...
                    final Message message = new Message();
                    message.setTarget(new Handler() {
                        @Override
                        public void handleMessage(Message msg) {
                            final Bundle data = msg.getData();
                            final String url = data.getString("url");
                            final String src = data.getString("src");

                            if (url == null || src == null) {
                                throw new IllegalStateException("WebView did not supply url or src for image link");
                            }

                            callback.onLongPress(new IWebView.ClickTarget(true, url, true, src), lastX, lastY);
                        }
                    });

                    webView.requestFocusNodeHref(message);
                    return true;

                case WebView.HitTestResult.IMAGE_TYPE:
                    final String imageURL = hitTestResult.getExtra();
                    callback.onLongPress(new IWebView.ClickTarget(false, null, true, imageURL), lastX, lastY);
                    return true;
            }

            return false;
        }
    }


    private static class WebkitView extends NestedWebView implements IWebView, SharedPreferences.OnSharedPreferenceChangeListener {
        private Callback callback;
        private FocusWebViewClient client;
        private final LinkHandler linkHandler;

        public WebkitView(Context context, AttributeSet attrs) {
            super(context, attrs);

            client = new FocusWebViewClient(getContext().getApplicationContext());

            setWebViewClient(client);
            setWebChromeClient(createWebChromeClient());
            setDownloadListener(createDownloadListener());

            if (BuildConfig.DEBUG) {
                setWebContentsDebuggingEnabled(true);
            }

            setLongClickable(true);

            linkHandler = new LinkHandler(this);
            setOnLongClickListener(linkHandler);
            setOnTouchListener(linkHandler);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();

            PreferenceManager.getDefaultSharedPreferences(getContext()).registerOnSharedPreferenceChangeListener(this);
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();

            PreferenceManager.getDefaultSharedPreferences(getContext()).unregisterOnSharedPreferenceChangeListener(this);
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            applyAppSettings(getContext(), getSettings());
        }

        @Override
        public void restoreWebviewState(Bundle savedInstanceState) {
            // We need to have a different method name because restoreState() returns
            // a WebBackForwardList, and we can't overload with different return types:
            final WebBackForwardList backForwardList = restoreState(savedInstanceState);

            // Pages are only added to the back/forward list when loading finishes. If a new page is
            // loading when the Activity is paused/killed, then that page won't be in the list,
            // and needs to be restored separately to the history list. We detect this by checking
            // whether the last fully loaded page (getCurrentItem()) matches the last page that the
            // WebView was actively loading (which was retrieved during onSaveInstanceState():
            // WebView.getUrl() always returns the currently loading or loaded page).
            // If the app is paused/killed before the initial page finished loading, then the entire
            // list will be null - so we need to additionally check whether the list even exists.

            final String desiredURL = savedInstanceState.getString(KEY_CURRENTURL);
            client.notifyCurrentURL(desiredURL);

            if (backForwardList != null &&
                    backForwardList.getCurrentItem().getUrl().equals(desiredURL)) {
                // restoreState doesn't actually load the current page, it just restores navigation history,
                // so we also need to explicitly reload in this case:
                reload();
            } else {
                loadUrl(desiredURL);
            }
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
            saveState(outState);
            // See restoreWebViewState() for an explanation of why we need to save this in _addition_
            // to WebView's state
            outState.putString(KEY_CURRENTURL, getUrl());
        }

        @Override
        public void setCallback(Callback callback) {
            this.callback = callback;
            client.setCallback(callback);
            linkHandler.setCallback(callback);
        }

        public void loadUrl(String url) {
            // We need to check external URL handling here - shouldOverrideUrlLoading() is only
            // called by webview when clicking on a link, and not when opening a new page for the
            // first time using loadUrl().
            if (!client.shouldOverrideUrlLoading(this, url)) {
                super.loadUrl(url);
            }

            client.notifyCurrentURL(url);
        }

        @Override
        public void destroy() {
            super.destroy();

            // WebView might save data to disk once it gets destroyed. In this case our cleanup call
            // might not have been able to see this data. Let's do it again.
            deleteContentFromKnownLocations();
        }

        @Override
        public void cleanup() {
            clearFormData();
            clearHistory();
            clearMatches();
            clearSslPreferences();
            clearCache(true);

            // We don't care about the callback - we just want to make sure cookies are gone
            CookieManager.getInstance().removeAllCookies(null);

            WebStorage.getInstance().deleteAllData();

            final WebViewDatabase webViewDatabase = WebViewDatabase.getInstance(getContext());
            // It isn't entirely clear how this differs from WebView.clearFormData()
            webViewDatabase.clearFormData();
            webViewDatabase.clearHttpAuthUsernamePassword();

            deleteContentFromKnownLocations();
        }

        private void deleteContentFromKnownLocations() {
            final Context context = getContext();

            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    // We call all methods on WebView to delete data. But some traces still remain
                    // on disk. This will wipe the whole webview directory.
                    FileUtils.deleteWebViewDirectory(context);

                    // WebView stores some files in the cache directory. We do not use it ourselves
                    // so let's truncate it.
                    FileUtils.truncateCacheDirectory(context);
                }
            });
        }

        private WebChromeClient createWebChromeClient() {
            return new WebChromeClient() {
                @Override
                public void onProgressChanged(WebView view, int newProgress) {
                    if (callback != null) {
                        // This is the earliest point where we might be able to confirm a redirected
                        // URL: we don't necessarily get a shouldInterceptRequest() after a redirect,
                        // so we can only check the updated url in onProgressChanges(), or in onPageFinished()
                        // (which is even later).
                        callback.onURLChanged(view.getUrl());
                        callback.onProgress(newProgress);
                    }
                }
            };
        }

        private DownloadListener createDownloadListener() {
            return new DownloadListener() {
                @Override
                public void onDownloadStart(String url, String userAgent, String contentDisposition, String mimetype, long contentLength) {
                    if (callback != null) {
                        final Download download = new Download(url, userAgent, contentDisposition, mimetype, contentLength);
                        callback.onDownloadStart(download);
                    }
                }
            };
        }
    }
}
