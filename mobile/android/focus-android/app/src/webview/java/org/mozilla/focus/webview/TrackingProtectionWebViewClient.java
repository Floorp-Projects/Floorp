/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webview;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.AsyncTask;
import android.support.annotation.WorkerThread;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.mozilla.focus.R;
import org.mozilla.focus.web.IWebView;
import org.mozilla.focus.webview.matcher.UrlMatcher;

public class TrackingProtectionWebViewClient extends WebViewClient {
    private static volatile UrlMatcher MATCHER;

    public static void triggerPreload(final Context context) {
        // Only trigger loading if MATCHER is null. (If it's null, MATCHER could already be loading,
        // but we don't have any way of being certain - and there's no real harm since we're not
        // blocking anything else.)
        if (MATCHER == null) {
            new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... voids) {
                    // We don't need the result here - we just want to trigger loading
                    getMatcher(context);
                    return null;
                }
            }.execute();
        }
    }

    @WorkerThread private static synchronized UrlMatcher getMatcher(final Context context) {
        if (MATCHER == null) {
            MATCHER = UrlMatcher.loadMatcher(context, R.raw.blocklist, new int[] { R.raw.google_mapping }, R.raw.entitylist);
        }
        return MATCHER;
    }

    private boolean blockingEnabled;
    /* package */ String currentPageURL;
    protected IWebView.Callback callback;

    /* package */ TrackingProtectionWebViewClient(final Context context) {
        // Hopefully we have loaded background data already. We call triggerPreload() to try to trigger
        // background loading of the lists as early as possible.
        triggerPreload(context);

        this.blockingEnabled = true;
    }

    public void setCallback(IWebView.Callback callback) {
        this.callback = callback;
    }

    public void setBlockingEnabled(boolean enabled) {
        this.blockingEnabled = enabled;
    }

    public boolean isBlockingEnabled() {
        return blockingEnabled;
    }

    @Override
    public WebResourceResponse shouldInterceptRequest(final WebView view, final WebResourceRequest request) {
        if (!blockingEnabled) {
            return super.shouldInterceptRequest(view, request);
        }

        final Uri resourceUri = request.getUrl();

        // shouldInterceptRequest() might be called _before_ onPageStarted or shouldOverrideUrlLoading
        // are called (this happens when the webview is first shown). However we are notified of the URL
        // via notifyCurrentURL in that case.
        final String scheme = resourceUri.getScheme();

        if (!request.isForMainFrame() &&
                !scheme.equals("http") && !scheme.equals("https")) {
            // Block any malformed non-http(s) URIs. WebView will already ignore things like market: URLs,
            // but not in all cases (malformed market: URIs, such as market:://... will still end up here).
            // (Note: data: URIs are automatically handled by WebView, and won't end up here either.)
            // file:// URIs are disabled separately by setting WebSettings.setAllowFileAccess()
            return new WebResourceResponse(null, null, null);
        }

        // WebView always requests a favicon, even though it won't be used anywhere. This check
        // isn't able to block all favicons (some of them will be loaded using <link rel="shortcut icon">
        // with a custom URL which we can't match or detect), but reduces the amount of unnecessary
        // favicon loading that's performed.
        final String path = resourceUri.getPath();
        if (path != null && path.endsWith("/favicon.ico")) {
            return new WebResourceResponse(null, null, null);
        }

        final UrlMatcher matcher = getMatcher(view.getContext());

        // Don't block the main frame from being loaded. This also protects against cases where we
        // open a link that redirects to another app (e.g. to the play store).
        if ((!request.isForMainFrame()) &&
                currentPageURL != null &&
                matcher.matches(resourceUri, Uri.parse(currentPageURL))) {
                // Bandaid for issue #26: currentPageUrl can still be null, and needs to be investigated further.
            if (callback != null) {
                callback.countBlockedTracker();
            }
            return new WebResourceResponse(null, null, null);
        }

        return super.shouldInterceptRequest(view, request);
    }

    /**
     * Notify that the user has requested a new URL. This MUST be called before loading a new URL
     * into the webview: sometimes content requests might begin before the WebView itself notifies
     * the WebViewClient via onpageStarted/shouldOverrideUrlLoading. If we don't know the current page
     * URL then the entitylist whitelists might not work if we're trying to load an explicitly whitelisted
     * page.
     */
    public void notifyCurrentURL(final String url) {
        currentPageURL = url;
    }

    @Override
    public void onPageStarted(WebView view, String url, Bitmap favicon) {
        if (callback != null) {
            callback.resetBlockedTrackers();
        }

        currentPageURL = url;

        super.onPageStarted(view, url, favicon);
    }
}
