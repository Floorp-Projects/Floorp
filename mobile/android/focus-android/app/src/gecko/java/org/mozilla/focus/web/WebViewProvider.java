/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

import org.mozilla.focus.browser.LocalizedContent;
import org.mozilla.focus.session.Session;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.utils.IntentUtils;
import org.mozilla.focus.utils.Settings;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;

import kotlin.text.Charsets;

/**
 * WebViewProvider implementation for creating a Gecko based implementation of IWebView.
 */
public class WebViewProvider {
    private static volatile GeckoRuntime geckoRuntime;

    public static void preload(final Context context) {
        createGeckoRuntimeIfNeeded(context);
    }

    public static View create(Context context, AttributeSet attrs) {
        return new GeckoWebView(context, attrs);
    }

    public static void performCleanup(final Context context) {
        // Nothing: does Gecko need extra private mode cleanup?
    }

    public static void performNewBrowserSessionCleanup() {
        // Nothing: a WebKit work-around.
    }

    private static void createGeckoRuntimeIfNeeded(Context context) {
        if (geckoRuntime == null) {
            final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                    new GeckoRuntimeSettings.Builder();
            runtimeSettingsBuilder.useContentProcessHint(true);
            geckoRuntime = GeckoRuntime.create(context.getApplicationContext(), runtimeSettingsBuilder.build());
        }
    }

    public static class GeckoWebView extends NestedGeckoView implements IWebView, SharedPreferences.OnSharedPreferenceChangeListener {
        private static final String TAG = "GeckoWebView";
        private Callback callback;
        private String currentUrl = "about:blank";
        private boolean canGoBack;
        private boolean canGoForward;
        private boolean isSecure;
        private final GeckoSession geckoSession;
        private String webViewTitle;

        public GeckoWebView(Context context, AttributeSet attrs) {
            super(context, attrs);

            final GeckoSessionSettings settings = new GeckoSessionSettings();
            settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, false);
            settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, true);

            geckoSession = new GeckoSession(settings);

            PreferenceManager.getDefaultSharedPreferences(context)
                    .registerOnSharedPreferenceChangeListener(this);

            applyAppSettings();
            updateBlocking();

            geckoSession.setContentDelegate(createContentDelegate());
            geckoSession.setProgressDelegate(createProgressDelegate());
            geckoSession.setNavigationDelegate(createNavigationDelegate());
            geckoSession.setTrackingProtectionDelegate(createTrackingProtectionDelegate());
            geckoSession.setPromptDelegate(createPromptDelegate());
            setSession(geckoSession, geckoRuntime);
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
            if (callback != null) {
                callback.onPageFinished(isSecure);
            }
        }

        @Override
        public String getUrl() {
            return currentUrl;
        }

        @Override
        public void loadUrl(final String url) {
            currentUrl = url;
            geckoSession.loadUri(currentUrl);
            if (callback != null) {
                callback.onProgress(10);
            }
        }

        @Override
        public void cleanup() {
            // We're running in a private browsing window, so nothing to do
        }

        @Override
        public void setBlockingEnabled(boolean enabled) {
            if (enabled) {
                updateBlocking();
                applyAppSettings();
            } else {
                if (geckoSession != null) {
                    geckoSession.disableTrackingProtection();
                    geckoRuntime.getSettings().setJavaScriptEnabled(true);
                    geckoRuntime.getSettings().setWebFontsEnabled(true);
                }
            }
            if (callback != null) {
                callback.onBlockingStateChanged(enabled);
            }
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String prefName) {
            updateBlocking();
            applyAppSettings();
        }

        private void applyAppSettings() {
            geckoRuntime.getSettings().setJavaScriptEnabled(!Settings.getInstance(getContext()).shouldBlockJavaScript());
            geckoRuntime.getSettings().setWebFontsEnabled(!Settings.getInstance(getContext()).shouldBlockWebFonts());
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
                    if (callback != null) {
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
                }

                @Override
                public void onContextMenu(GeckoSession session, int screenX, int screenY, String uri, @ElementType int elementType, String elementSrc) {
                    if (callback != null) {
                        if (elementSrc != null && uri != null && elementType ==
                                ELEMENT_TYPE_IMAGE) {
                            callback.onLongPress(new HitTarget(true, uri, true, elementSrc));
                        } else if (elementSrc != null && elementType == ELEMENT_TYPE_IMAGE) {
                            callback.onLongPress(new HitTarget(false, null, true, elementSrc));
                        } else if (uri != null) {
                            callback.onLongPress(new HitTarget(true, uri, false, null));
                        }
                    }
                }

                @Override
                public void onExternalResponse(GeckoSession session, GeckoSession.WebResponseInfo response) {
                    if (!AppConstants.supportsDownloadingFiles()) {
                        return;
                    }

                    final String scheme = Uri.parse(response.uri).getScheme();
                    if (scheme == null || (!scheme.equals("http") && !scheme.equals("https"))) {
                        // We are ignoring everything that is not http or https. This is a limitation of
                        // Android's download manager. There's no reason to show a download dialog for
                        // something we can't download anyways.
                        Log.w(TAG, "Ignoring download from non http(s) URL: " + response.uri);
                        return;
                    }

                    if (callback != null) {
                        // TODO: get user agent from GeckoView #2470
                        final Download download = new Download(response.uri, "Mozilla/5.0 (Android 8.1.0; Mobile; rv:60.0) Gecko/60.0 Firefox/60.0",
                                response.filename, response.contentType, response.contentLength,
                                Environment.DIRECTORY_DOWNLOADS);
                        callback.onDownloadStart(download);
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
                    if (callback != null) {
                        callback.onSecurityChanged(isSecure, securityInfo.host, securityInfo.issuerOrganization);
                    }
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
                public void onLoadRequest(GeckoSession session, String uri, int target, GeckoSession.Response<Boolean> response) {
                    // If this is trying to load in a new tab, just load it in the current one
                    if (target == GeckoSession.NavigationDelegate.TARGET_WINDOW_NEW) {
                        geckoSession.loadUri(uri);
                        response.respond(true);
                    }

                    // Check if we should handle an internal link
                    if (LocalizedContent.handleInternalContent(uri, GeckoWebView.this, getContext())) {
                        response.respond(true);
                    }

                    // Check if we should handle an external link
                    final Uri urlToURI = Uri.parse(uri);
                    if (!UrlUtils.isSupportedProtocol(urlToURI.getScheme()) && callback != null &&
                            IntentUtils.handleExternalUri(getContext(), GeckoWebView.this, uri)) {
                        response.respond(true);
                    }

                    if (uri.equals("about:neterror") || uri.equals("about:certerror")) {
                        // TODO: Error Page handling with Components ErrorPages #2471
                        response.respond(true);
                    }

                    // Otherwise allow the load to continue normally
                    response.respond(false);
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
        public void loadData(String baseURL, String data, String mimeType, String encoding, String historyURL) {
            geckoSession.loadData(data.getBytes(Charsets.UTF_8), mimeType, baseURL);
        }

        @Override
        public void onDetachedFromWindow() {
            PreferenceManager.getDefaultSharedPreferences(getContext()).unregisterOnSharedPreferenceChangeListener(this);
            super.onDetachedFromWindow();
        }
    }
}
