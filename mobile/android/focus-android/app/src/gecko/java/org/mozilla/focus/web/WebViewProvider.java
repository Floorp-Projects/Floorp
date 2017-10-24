/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.content.Context;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.view.View;

import org.mozilla.focus.session.Session;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoViewSettings;

/**
 * WebViewProvider implementation for creating a Gecko based implementation of IWebView.
 */
public class WebViewProvider {
    public static void preload(final Context context) {
        GeckoView.preload(context);
    }

    public static View create(Context context, AttributeSet attrs) {
        final GeckoViewSettings settings = new GeckoViewSettings();
        settings.setBoolean(GeckoViewSettings.USE_MULTIPROCESS, false);
        settings.setBoolean(GeckoViewSettings.USE_PRIVATE_MODE, true);
        settings.setBoolean(GeckoViewSettings.USE_TRACKING_PROTECTION, true);
        final GeckoView geckoView = new GeckoWebView(context, attrs, settings);

        return geckoView;
    }

    public static void performCleanup(final Context context) {
        // Nothing: does Gecko need extra private mode cleanup?
    }

    public static void performNewBrowserSessionCleanup() {
        // Nothing: a WebKit work-around.
    }

    public static class GeckoWebView extends NestedGeckoView implements IWebView {
        private Callback callback;
        private String currentUrl = "about:blank";
        private boolean canGoBack;
        private boolean canGoForward;
        private boolean isSecure;

        public GeckoWebView(Context context, AttributeSet attrs, GeckoViewSettings settings) {
            super(context, attrs, settings);

            setContentListener(createContentListener());
            setProgressListener(createProgressListener());
            setNavigationListener(createNavigationListener());

            // TODO: set long press listener, call through to callback.onLinkLongPress()
        }

        @Override
        public void setCallback(Callback callback) {
            this.callback =  callback;
        }

        @Override
        public void onPause() {

        }

        @Override
        public void onResume() {

        }

        @Override
        public void stopLoading() {
            this.stop();
            callback.onPageFinished(isSecure);
        }

        @Override
        public String getUrl() {
            return currentUrl;
        }

        @Override
        public void loadUrl(final String url) {
            currentUrl = url;
            loadUri(currentUrl);
        }

        @Override
        public void cleanup() {
            // We're running in a private browsing window, so nothing to do
        }

        @Override
        public void setBlockingEnabled(boolean enabled) {
            // We can't actually do this?
        }

        private ContentListener createContentListener() {
            return new ContentListener() {
                @Override
                public void onTitleChange(GeckoView geckoView, String s) {
                }

                @Override
                public void onFullScreen(GeckoView geckoView, boolean fullScreen) {
                }
            };
        }

        private ProgressListener createProgressListener() {
            return new ProgressListener() {
                @Override
                public void onPageStart(GeckoView geckoView, String url) {
                    if (callback != null) {
                        callback.onPageStarted(url);
                        callback.onProgress(25);
                        isSecure = false;
                    }
                }

                @Override
                public void onPageStop(GeckoView geckoView, boolean success) {
                    if (callback != null) {
                        if (success) {
                            callback.onProgress(100);
                            callback.onPageFinished(isSecure);
                        }
                    }
                }

                @Override
                public void onSecurityChange(GeckoView geckoView, int status) {
                    // TODO: Split current onPageFinished() callback into two: page finished + security changed
                    isSecure = status == ProgressListener.STATE_IS_SECURE;
                }
            };
        }

        private NavigationListener createNavigationListener() {
            return new NavigationListener() {
                public void onLocationChange(GeckoView view, String url) {
                    if (callback != null) {
                        callback.onURLChanged(url);
                    }
                }

                public void onCanGoBack(GeckoView view, boolean canGoBack) {
                    GeckoWebView.this.canGoBack =  canGoBack;
                }

                public void onCanGoForward(GeckoView view, boolean canGoForward) {
                    GeckoWebView.this.canGoForward = canGoForward;
                }
            };
        }

        @Override
        public boolean canGoForward() {
            return canGoForward;
        }

        @Override
        public boolean canGoBack() {
            return canGoBack;
        }

        @Override
        public void restoreWebViewState(Session session) {
            // TODO: restore navigation history, and reopen previously opened page
        }

        @Override
        public void saveWebViewState(@NonNull Session session) {
            // TODO: save anything needed for navigation history restoration.
        }

        @Override
        public String getTitle() {
            return "?";
        }
    }
}
