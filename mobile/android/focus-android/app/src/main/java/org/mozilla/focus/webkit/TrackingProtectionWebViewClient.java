/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit;

import android.content.Context;
import android.graphics.Bitmap;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.mozilla.focus.R;
import org.mozilla.focus.webkit.matcher.UrlMatcher;

public class TrackingProtectionWebViewClient extends WebViewClient {

    private String currentPageURL;
    private final UrlMatcher matcher;

    public TrackingProtectionWebViewClient(final Context context) {
        matcher = new UrlMatcher(context, R.raw.blocklist, R.raw.entitylist);
    }

    /*
     * The new undeprecated version of this methods is API 21+.
     */
    @Override
    public WebResourceResponse shouldInterceptRequest(WebView view, String url) {
        if (matcher.matches(url, currentPageURL)) {
            return new WebResourceResponse(null, null, null);
        }

        return super.shouldInterceptRequest(view, url);
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
        currentPageURL = url;

        super.onPageStarted(view, url, favicon);
    }

    @Override
    public boolean shouldOverrideUrlLoading(WebView view, String url) {
        currentPageURL = url;

        return super.shouldOverrideUrlLoading(view, url);
    }

}
