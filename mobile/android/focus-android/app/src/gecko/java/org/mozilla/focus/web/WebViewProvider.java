/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.view.View;

import org.mozilla.focus.session.Session;
import org.mozilla.focus.utils.IntentUtils;
import org.mozilla.focus.utils.Settings;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;

/**
 * WebViewProvider implementation for creating a Gecko based implementation of IWebView.
 */
public class WebViewProvider {
    public static void preload(final Context context) {
        GeckoSession.preload(context);
    }

    public static View create(Context context, AttributeSet attrs) {
        final GeckoView geckoView = new GeckoWebView(context, attrs);
        return geckoView;
    }

    public static void performCleanup(final Context context) {
        // Nothing: does Gecko need extra private mode cleanup?
    }

    public static void performNewBrowserSessionCleanup() {
        // Nothing: a WebKit work-around.
    }

    public static class GeckoWebView extends NestedGeckoView implements IWebView, SharedPreferences.OnSharedPreferenceChangeListener {
        private Callback callback;
        private String currentUrl = "about:blank";
        private boolean canGoBack;
        private boolean canGoForward;
        private boolean isSecure;
        private GeckoSession geckoSession;
        private String webViewTitle;

        public GeckoWebView(Context context, AttributeSet attrs) {
            super(context, attrs);

            final GeckoSessionSettings settings = new GeckoSessionSettings();
            settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, false);
            settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, true);

            geckoSession = new GeckoSession(settings);

            PreferenceManager.getDefaultSharedPreferences(context)
                    .registerOnSharedPreferenceChangeListener(this);

            updateBlocking();

            geckoSession.setContentDelegate(createContentDelegate());
            geckoSession.setProgressDelegate(createProgressDelegate());
            geckoSession.setNavigationDelegate(createNavigationDelegate());
            geckoSession.setTrackingProtectionDelegate(createTrackingProtectionDelegate());
            geckoSession.setPromptDelegate(createPromptDelegate());
            setSession(geckoSession);
        }

        @Override
        public void setCallback(Callback callback) {
            this.callback =  callback;
        }

        @Override
        public void onPause() {

        }

        @Override
        public void goBack() {
            geckoSession.goBack();
        }

        @Override
        public void goForward() {
            geckoSession.goForward();
        }

        @Override
        public void reload() {
            geckoSession.reload();
        }

        @Override
        public void destroy() {
            geckoSession.close();
        }

        @Override
        public void onResume() {

        }

        @Override
        public void stopLoading() {
            geckoSession.stop();
            callback.onPageFinished(isSecure);
        }

        @Override
        public String getUrl() {
            return currentUrl;
        }

        @Override
        public void loadUrl(final String url) {
            currentUrl = url;
            geckoSession.loadUri(currentUrl);
            callback.onProgress(10);
        }

        @Override
        public void cleanup() {
            // We're running in a private browsing window, so nothing to do
        }

        @Override
        public void setBlockingEnabled(boolean enabled) {
            if (enabled) {
                updateBlocking();
            } else {
                if (geckoSession != null) {
                    geckoSession.disableTrackingProtection();
                }
            }
            if (callback != null) {
                callback.onBlockingStateChanged(enabled);
            }
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String prefName) {
            updateBlocking();
        }

        private void updateBlocking() {
            final Settings settings = Settings.getInstance(getContext());

            int categories = 0;
            if (settings.shouldBlockSocialTrackers()) {
                categories += GeckoSession.TrackingProtectionDelegate.CATEGORY_SOCIAL;
            }
            if (settings.shouldBlockAdTrackers()) {
                categories += GeckoSession.TrackingProtectionDelegate.CATEGORY_AD;
            }
            if (settings.shouldBlockAnalyticTrackers()) {
                categories += GeckoSession.TrackingProtectionDelegate.CATEGORY_ANALYTIC;
            }
            if (settings.shouldBlockOtherTrackers()) {
                categories += GeckoSession.TrackingProtectionDelegate.CATEGORY_CONTENT;
            }
            if (geckoSession != null) {
                geckoSession.enableTrackingProtection(categories);
            }
        }

        private GeckoSession.ContentDelegate createContentDelegate() {
            return new GeckoSession.ContentDelegate() {
                @Override
                public void onTitleChange(GeckoSession session, String title) {
                    webViewTitle = title;
                }

                @Override
                public void onFullScreen(GeckoSession session, boolean fullScreen) {
                    if (fullScreen) {
                        callback.onEnterFullScreen(new FullscreenCallback() {
                            @Override
                            public void fullScreenExited() {
                                geckoSession.exitFullScreen();
                            }
                        }, null);
                    } else {
                        callback.onExitFullScreen();
                    }
                }

                @Override
                public void onContextMenu(GeckoSession session, int screenX, int screenY, String uri, @ElementType int elementType, String elementSrc) {
                    if (elementSrc != null && uri != null) {
                        callback.onLongPress(new HitTarget(true, uri, true, elementSrc));
                    } else if (elementSrc != null) {
                        if (elementSrc.endsWith("jpg") || elementSrc.endsWith("gif") ||
                                elementSrc.endsWith("tif") || elementSrc.endsWith("bmp") ||
                                elementSrc.endsWith("png")) {
                            callback.onLongPress(new HitTarget(false, null, true, elementSrc));
                        }
                    } else if (uri != null) {
                        callback.onLongPress(new HitTarget(true, uri, false, null));
                    }
                }

                @Override
                public void onFocusRequest(GeckoSession geckoSession) {

                }

                @Override
                public void onCloseRequest(GeckoSession geckoSession) {
                    // TODO: #2150
                }
            };
        }

        private GeckoSession.ProgressDelegate createProgressDelegate() {
            return new GeckoSession.ProgressDelegate() {
                @Override
                public void onPageStart(GeckoSession session, String url) {
                    if (callback != null) {
                        callback.onPageStarted(url);
                        callback.resetBlockedTrackers();
                        callback.onProgress(25);
                        isSecure = false;
                    }
                }

                @Override
                public void onPageStop(GeckoSession session, boolean success) {
                    if (callback != null) {
                        if (success) {
                            callback.onProgress(100);
                            callback.onPageFinished(isSecure);
                        }
                    }
                }

                @Override
                public void onSecurityChange(GeckoSession session,
                                             GeckoSession.ProgressDelegate.SecurityInformation securityInfo) {
                    isSecure = securityInfo.isSecure;
                    callback.onSecurityChanged(isSecure, securityInfo.host, securityInfo.issuerOrganization);
                }
            };
        }

        private GeckoSession.NavigationDelegate createNavigationDelegate() {
            return new GeckoSession.NavigationDelegate() {
                public void onLocationChange(GeckoSession session, String url) {
                    currentUrl = url;
                    if (callback != null) {
                        callback.onURLChanged(url);
                    }
                }

                public void onCanGoBack(GeckoSession session, boolean canGoBack) {
                    GeckoWebView.this.canGoBack =  canGoBack;
                }

                public void onCanGoForward(GeckoSession session, boolean canGoForward) {
                    GeckoWebView.this.canGoForward = canGoForward;
                }

                @Override
                public boolean onLoadRequest(GeckoSession session, String uri, @TargetWindow int target) {
                    // If this is trying to load in a new tab, just load it in the current one
                    if (target == GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW) {
                        geckoSession.loadUri(uri);
                        return true;
                    }

                    // Check if we should handle an internal link
                    if (LocalizedContentGecko.INSTANCE.handleInternalContent(uri, session, getContext())) {
                        return true;
                    }

                    // Check if we should handle an external link
                    final Uri urlToURI = Uri.parse(uri);
                    if (!UrlUtils.isSupportedProtocol(urlToURI.getScheme()) && callback != null) {
                        return IntentUtils.handleExternalUri(getContext(), GeckoWebView.this, uri);
                    }

                    // Otherwise allow the load to continue normally
                    return false;
                }

                @Override
                public void onNewSession(GeckoSession geckoSession, String s, GeckoSession.Response<GeckoSession> response) {
                    // TODO: #2151
                }
            };
        }

        private GeckoSession.TrackingProtectionDelegate createTrackingProtectionDelegate() {
           return new GeckoSession.TrackingProtectionDelegate() {
                @Override
                public void onTrackerBlocked(GeckoSession geckoSession, String s, int i) {
                    if (callback != null) {
                        callback.countBlockedTracker();
                    }
                }
            };
        }

        private GeckoSession.PromptDelegate createPromptDelegate() {
            return new GeckoViewPrompt((Activity) getContext());
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
            return webViewTitle;
        }

        @Override
        public void exitFullscreen() {
            geckoSession.exitFullScreen();
        }

        @Override
        public void onDetachedFromWindow() {
            PreferenceManager.getDefaultSharedPreferences(getContext()).unregisterOnSharedPreferenceChangeListener(this);
            super.onDetachedFromWindow();
        }
    }
}
