/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test;

import org.json.JSONObject;
import org.mozilla.geckoview.AllowOrDeny;
import org.mozilla.geckoview.GeckoDisplay;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.WebExtension;
import org.mozilla.geckoview.WebExtensionController;
import org.mozilla.geckoview.WebRequestError;

import android.app.Activity;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.Surface;

import java.util.HashMap;
import java.util.HashSet;

public class TestRunnerActivity extends Activity {
    private static final String LOGTAG = "TestRunnerActivity";
    private static final String ERROR_PAGE =
            "<!DOCTYPE html><head><title>Error</title></head><body>Error!</body></html>";

    static GeckoRuntime sRuntime;

    private GeckoSession mSession;
    private GeckoView mView;
    private boolean mKillProcessOnDestroy;

    private HashMap<GeckoSession, Display> mDisplays = new HashMap<>();

    private static class Display {
        public final SurfaceTexture texture;
        public final Surface surface;

        private final int width;
        private final int height;
        private GeckoDisplay sessionDisplay;

        public Display(final int width, final int height) {
            this.width = width;
            this.height = height;
            texture = new SurfaceTexture(0);
            texture.setDefaultBufferSize(width, height);
            surface = new Surface(texture);
        }

        public void attach(final GeckoSession session) {
            sessionDisplay = session.acquireDisplay();
            sessionDisplay.surfaceChanged(surface, width, height);
        }

        public void release(final GeckoSession session) {
            sessionDisplay.surfaceDestroyed();
            session.releaseDisplay(sessionDisplay);
        }
    }

    private HashSet<GeckoSession> mOwnedSessions = new HashSet<>();

    private GeckoSession.PermissionDelegate mPermissionDelegate = new GeckoSession.PermissionDelegate() {
        @Override
        public void onContentPermissionRequest(@NonNull GeckoSession session, @Nullable String uri, int type, @NonNull Callback callback) {
            callback.grant();
        }

        @Override
        public void onAndroidPermissionsRequest(@NonNull GeckoSession session, @Nullable String[] permissions, @NonNull Callback callback) {
            callback.grant();
        }
    };

    private GeckoSession.NavigationDelegate mNavigationDelegate = new GeckoSession.NavigationDelegate() {
        @Override
        public void onLocationChange(GeckoSession session, String url) {
            getActionBar().setSubtitle(url);
        }

        @Override
        public void onCanGoBack(GeckoSession session, boolean canGoBack) {

        }

        @Override
        public void onCanGoForward(GeckoSession session, boolean canGoForward) {

        }

        @Override
        public GeckoResult<AllowOrDeny> onLoadRequest(GeckoSession session,
                                                  LoadRequest request) {
            // Allow Gecko to load all URIs
            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }

        @Override
        public GeckoResult<GeckoSession> onNewSession(GeckoSession session, String uri) {
            return GeckoResult.fromValue(createBackgroundSession(session.getSettings()));
        }

        @Override
        public GeckoResult<String> onLoadError(GeckoSession session, String uri, WebRequestError error) {

            return GeckoResult.fromValue("data:text/html," + ERROR_PAGE);
        }
    };

    private GeckoSession.ContentDelegate mContentDelegate = new GeckoSession.ContentDelegate() {
        private void onContentProcessGone() {
            if (System.getenv("MOZ_CRASHREPORTER_SHUTDOWN") != null) {
                sRuntime.shutdown();
            }
        }

        @Override
        public void onTitleChange(GeckoSession session, String title) {

        }

        @Override
        public void onFocusRequest(GeckoSession session) {

        }

        @Override
        public void onCloseRequest(GeckoSession session) {
            closeSession(session);
        }

        @Override
        public void onFullScreen(GeckoSession session, boolean fullScreen) {

        }

        @Override
        public void onContextMenu(GeckoSession session, int screenX, int screenY,
                                  ContextElement element) {

        }

        @Override
        public void onExternalResponse(GeckoSession session, GeckoSession.WebResponseInfo request) {
        }

        @Override
        public void onCrash(GeckoSession session) {
            onContentProcessGone();
        }

        @Override
        public void onKill(GeckoSession session) {
            onContentProcessGone();
        }

        @Override
        public void onFirstComposite(final GeckoSession session) {
        }

        @Override
        public void onWebAppManifest(final GeckoSession session, final JSONObject manifest) {
        }
    };

    private GeckoSession createSession() {
        return createSession(null);
    }

    private GeckoSession createSession(GeckoSessionSettings settings) {
        if (settings == null) {
            settings = new GeckoSessionSettings();
        }

        final GeckoSession session = new GeckoSession(settings);
        session.setNavigationDelegate(mNavigationDelegate);
        session.setContentDelegate(mContentDelegate);
        session.setPermissionDelegate(mPermissionDelegate);
        mOwnedSessions.add(session);
        return session;
    }

    private GeckoSession createBackgroundSession(final GeckoSessionSettings settings) {
        final GeckoSession session = createSession(settings);

        final Display display = new Display(mView.getWidth(), mView.getHeight());
        display.attach(session);

        mDisplays.put(session, display);

        return session;
    }

    private void closeSession(GeckoSession session) {
        if (mDisplays.containsKey(session)) {
            final Display display = mDisplays.remove(session);
            display.release(session);
        }
        mOwnedSessions.remove(session);
        session.close();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();

        if (sRuntime == null) {
            final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                new GeckoRuntimeSettings.Builder();

            // Mochitest and reftest encounter rounding errors if we have a
            // a window.devicePixelRation like 3.625, so simplify that here.
            runtimeSettingsBuilder
                    .arguments(new String[] { "-purgecaches" })
                    .displayDpiOverride(160)
                    .displayDensityOverride(1.0f)
                    .remoteDebuggingEnabled(true)
                    .autoplayDefault(GeckoRuntimeSettings.AUTOPLAY_DEFAULT_ALLOWED);

            final Bundle extras = intent.getExtras();
            if (extras != null) {
                runtimeSettingsBuilder.extras(extras);
            }

            runtimeSettingsBuilder
                    .consoleOutput(true)
                    .crashHandler(TestCrashHandler.class);

            sRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());

            sRuntime.getWebExtensionController().setTabDelegate(new WebExtensionController.TabDelegate() {
                @Override
                public GeckoResult<GeckoSession> onNewTab(WebExtension source, String uri) {
                    return GeckoResult.fromValue(createSession());
                }
                @Override
                public GeckoResult<AllowOrDeny> onCloseTab(WebExtension source, GeckoSession session) {
                   closeSession(session);
                   return GeckoResult.fromValue(AllowOrDeny.ALLOW);
                }
            });
            sRuntime.setDelegate(() -> {
                mKillProcessOnDestroy = true;
                finish();
            });
        }

        mSession = createSession();
        mSession.open(sRuntime);

        // If we were passed a URI in the Intent, open it
        final Uri uri = intent.getData();
        if (uri != null) {
            mSession.loadUri(uri);
        }

        mView = new GeckoView(this);
        mView.setSession(mSession);
        setContentView(mView);
    }

    @Override
    protected void onDestroy() {
        mSession.close();
        super.onDestroy();

        if (mKillProcessOnDestroy) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }

    public GeckoView getGeckoView() {
        return mView;
    }

    public GeckoSession getGeckoSession() {
        return mSession;
    }
}
