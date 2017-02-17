/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.View;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;

import org.mozilla.focus.webkit.TrackingProtectionWebViewClient;

/**
 * WebViewProvider for creating a WebKit based IWebVIew implementation.
 */
public class WebViewProvider {
    /**
     * Preload webview data. This allows the webview implementation to load resources and other data
     * it might need, in advance of intialising the view (at which time we are probably wanting to
     * show a website immediately).
     */
    public static void preload(final Context context) {
        TrackingProtectionWebViewClient.triggerPreload(context);
    }

    public static View create(Context context, AttributeSet attrs) {
        final WebkitView webkitView = new WebkitView(context, attrs);

        setupView(webkitView);
        configureSettings(webkitView.getSettings());

        return webkitView;
    }

    private static void setupView(WebView webView) {
        webView.setVerticalScrollBarEnabled(true);
        webView.setHorizontalScrollBarEnabled(true);
    }

    @SuppressLint("SetJavaScriptEnabled") // We explicitly want to enable JavaScript
    private static void configureSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);

        // Enabling built in zooming shows the controls by default
        settings.setBuiltInZoomControls(true);

        // So we hide the controls after enabling zooming
        settings.setDisplayZoomControls(false);
    }

    private static class WebkitView extends WebView implements IWebView {
        private Callback callback;
        private TrackingProtectionWebViewClient client;

        public WebkitView(Context context, AttributeSet attrs) {
            super(context, attrs);

            client = createWebViewClient();

            setWebViewClient(client);
            setWebChromeClient(createWebChromeClient());
        }

        @Override
        public void setCallback(Callback callback) {
            this.callback = callback;
        }

        public void loadUrl(String url) {
            super.loadUrl(url);

            client.notifyCurrentURL(url);
        }

        @Override
        public void cleanup() {
            clearFormData();
            clearHistory();
            clearMatches();
            clearSslPreferences();
            clearCache(true);
        }

        private TrackingProtectionWebViewClient createWebViewClient() {
            return new TrackingProtectionWebViewClient(getContext().getApplicationContext()) {
                @Override
                public void onPageStarted(WebView view, String url, Bitmap favicon) {
                    if (callback != null) {
                        callback.onPageStarted(url);
                    }
                    super.onPageStarted(view, url, favicon);
                }

                @Override
                public void onPageFinished(WebView view, String url) {
                    if (callback != null) {
                        callback.onPageFinished(view.getCertificate() != null);
                    }
                    super.onPageFinished(view, url);
                }
            };
        }

        private WebChromeClient createWebChromeClient() {
            return new WebChromeClient() {
                @Override
                public void onProgressChanged(WebView view, int newProgress) {
                    if (callback != null) {
                        callback.onProgress(newProgress);
                    }
                }
            };
        }
    }
}
