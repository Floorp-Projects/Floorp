/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.web;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.focus.browser.LocalizedContent;
import org.mozilla.focus.session.Session;
import org.mozilla.focus.telemetry.SentryWrapper;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.utils.IntentUtils;
import org.mozilla.focus.utils.Settings;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.GeckoResponse;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;

import java.util.concurrent.CountDownLatch;

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
            runtimeSettingsBuilder.nativeCrashReportingEnabled(true);
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
        private GeckoSession geckoSession;
        private String webViewTitle;
        private boolean isLoadingInternalUrl = false;
        private String internalAboutData = null;
        private String internalRightsData = null;

        public GeckoWebView(Context context, AttributeSet attrs) {
            super(context, attrs);
            PreferenceManager.getDefaultSharedPreferences(context)
                    .registerOnSharedPreferenceChangeListener(this);
            geckoSession = createGeckoSession();
            applySettingsAndSetDelegates();
            setSession(geckoSession, geckoRuntime);
        }

        private void applySettingsAndSetDelegates() {
            applyAppSettings();
            updateBlocking();

            geckoSession.setContentDelegate(createContentDelegate());
            geckoSession.setProgressDelegate(createProgressDelegate());
            geckoSession.setNavigationDelegate(createNavigationDelegate());
            geckoSession.setTrackingProtectionDelegate(createTrackingProtectionDelegate());
            geckoSession.setPromptDelegate(createPromptDelegate());
        }

        private GeckoSession createGeckoSession() {
            final GeckoSessionSettings settings = new GeckoSessionSettings();
            settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, true);
            settings.setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, true);

            return new GeckoSession(settings);
        }

        @Override
        public void setCallback(Callback callback) {
            this.callback = callback;
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
            if (TelemetryWrapper.dayPassedSinceLastUpload(getContext())) {
                sendTelemetrySnapshots();
            }
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
            geckoSession.getSettings().setBoolean(GeckoSessionSettings.USE_TRACKING_PROTECTION, enabled);
            if (enabled) {
                updateBlocking();
                applyAppSettings();
            } else {
                if (geckoSession != null) {
                    geckoRuntime.getSettings().setJavaScriptEnabled(true);
                    geckoRuntime.getSettings().setWebFontsEnabled(true);
                }
            }
            if (callback != null) {
                callback.onBlockingStateChanged(enabled);
            }
        }

        @Override
        public void setRequestDesktop(boolean shouldRequestDesktop) {
            geckoSession.getSettings().setBoolean(GeckoSessionSettings.USE_DESKTOP_MODE, shouldRequestDesktop);
            if (callback != null) {
                callback.onRequestDesktopStateChanged(shouldRequestDesktop);
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
            geckoRuntime.getSettings().setRemoteDebuggingEnabled(false);
            final int cookiesValue;
            if (Settings.getInstance(getContext()).shouldBlockCookies() && Settings.getInstance(getContext()).shouldBlockThirdPartyCookies()) {
                cookiesValue = GeckoRuntimeSettings.COOKIE_ACCEPT_NONE;
            } else if (Settings.getInstance(getContext()).shouldBlockThirdPartyCookies()) {
                cookiesValue = GeckoRuntimeSettings.COOKIE_ACCEPT_FIRST_PARTY;
            } else {
                cookiesValue = GeckoRuntimeSettings.COOKIE_ACCEPT_ALL;
            }
            geckoRuntime.getSettings().setCookieBehavior(cookiesValue);
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

            geckoRuntime.getSettings().setTrackingProtectionCategories(categories);
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
                public void onCrash(GeckoSession session) {
                    Log.i(TAG, "Crashed, opening new session");
                    SentryWrapper.INSTANCE.captureGeckoCrash();
                    geckoSession.close();
                    geckoSession = createGeckoSession();
                    applySettingsAndSetDelegates();
                    geckoSession.open(geckoRuntime);
                    setSession(geckoSession);
                    geckoSession.loadUri(currentUrl);
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
                            if (UrlUtils.isLocalizedContent(getUrl())) {
                                // When the url is a localized content, then the page is secure
                                isSecure = true;
                            }

                            callback.onProgress(100);
                            callback.onPageFinished(isSecure);
                        }
                    }
                }

                @Override
                public void onSecurityChange(GeckoSession session,
                                             GeckoSession.ProgressDelegate.SecurityInformation securityInfo) {
                    isSecure = securityInfo.isSecure;

                    if (UrlUtils.isLocalizedContent(getUrl())) {
                        // When the url is a localized content, then the page is secure
                        isSecure = true;
                    }

                    if (callback != null) {
                        callback.onSecurityChanged(isSecure, securityInfo.host, securityInfo.issuerOrganization);
                    }
                }
            };
        }

        private GeckoSession.NavigationDelegate createNavigationDelegate() {
            return new GeckoSession.NavigationDelegate() {
                public void onLocationChange(GeckoSession session, String url) {
                    // Save internal data: urls we should override to present focus:about, focus:rights
                    if (isLoadingInternalUrl) {
                        if (currentUrl.equals(LocalizedContent.URL_ABOUT)) {
                            internalAboutData = url;
                        } else if (currentUrl.equals(LocalizedContent.URL_RIGHTS)) {
                            internalRightsData = url;
                        }
                        isLoadingInternalUrl = false;
                        url = currentUrl;
                    }

                    // Check for internal data: urls to instead present focus:rights, focus:about
                    if (!TextUtils.isEmpty(internalAboutData) && internalAboutData.equals(url)) {
                        url = LocalizedContent.URL_ABOUT;
                    } else if (!TextUtils.isEmpty(internalRightsData) && internalRightsData.equals(url)) {
                        url = LocalizedContent.URL_RIGHTS;
                    }

                    currentUrl = url;
                    if (callback != null) {
                        callback.onURLChanged(url);
                    }
                }

                public void onCanGoBack(GeckoSession session, boolean canGoBack) {
                    GeckoWebView.this.canGoBack = canGoBack;
                }

                public void onCanGoForward(GeckoSession session, boolean canGoForward) {
                    GeckoWebView.this.canGoForward = canGoForward;
                }

                @Override
                public void onLoadRequest(GeckoSession session, String uri, int target, int flags, GeckoResponse<Boolean> response) {
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

                    if (callback != null) {
                        callback.onRequest(flags == GeckoSession.NavigationDelegate.LOAD_REQUEST_IS_USER_TRIGGERED);
                    }

                    // Otherwise allow the load to continue normally
                    response.respond(false);
                }

                @Override
                public void onNewSession(GeckoSession session, String uri, GeckoResponse<GeckoSession> response) {
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
            final Bundle stateData = session.getWebViewState();
            final String desiredURL = session.getUrl().getValue();
            final GeckoSession.SessionState sessionState = stateData.getParcelable("state");
            if (sessionState != null) {
                geckoSession.restoreState(sessionState);
            } else {
                loadUrl(desiredURL);
            }
        }

        @Override
        public void saveWebViewState(@NonNull final Session session) {
            final CountDownLatch latch = new CountDownLatch(1);
            saveStateInBackground(latch, session);
            try {
                latch.await();
            } catch (InterruptedException e) {
                // State was not saved
            }
        }

        private void saveStateInBackground(final CountDownLatch latch, final Session session) {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    final GeckoResponse<GeckoSession.SessionState> response = new GeckoResponse<GeckoSession.SessionState>() {
                        @Override
                        public void respond(GeckoSession.SessionState value) {
                            if (value != null) {
                                final Bundle bundle = new Bundle();
                                bundle.putParcelable("state", value);
                                session.saveWebViewState(bundle);
                            }
                            latch.countDown();
                        }
                    };

                    geckoSession.saveState(response);
                }
            });
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
        public void findAllAsync(String find) {
            // TODO: #2690
        }

        @Override
        public void findNext(boolean forward) {
            // TODO: #2690
        }

        @Override
        public void clearMatches() {
            // TODO: #2690
        }

        @Override
        public void setFindListener(IFindListener findListener) {
            // TODO: #2690
        }

        @Override
        public void loadData(String baseURL, String data, String mimeType, String encoding, String historyURL) {
            geckoSession.loadData(data.getBytes(Charsets.UTF_8), mimeType, baseURL);
            currentUrl = baseURL;
            isLoadingInternalUrl = currentUrl.equals(LocalizedContent.URL_RIGHTS) || currentUrl.equals(LocalizedContent.URL_ABOUT);
        }

        private void sendTelemetrySnapshots() {
            final GeckoResponse<GeckoBundle> response = new GeckoResponse<GeckoBundle>() {
                @Override
                public void respond(GeckoBundle value) {
                    if (value != null) {
                        try {
                            final JSONObject jsonData = value.toJSONObject();
                            TelemetryWrapper.addMobileMetricsPing(jsonData);
                        } catch (JSONException e) {
                            Log.e("getSnapshots failed", e.getMessage());
                        }
                    }
                }
            };

            geckoRuntime.getTelemetry().getSnapshots(true, response);
        }

        @Override
        public void onDetachedFromWindow() {
            PreferenceManager.getDefaultSharedPreferences(getContext()).unregisterOnSharedPreferenceChangeListener(this);
            super.onDetachedFromWindow();
        }
    }
}
