/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test;

import org.mozilla.gecko.gfx.GeckoDisplay;
import org.mozilla.geckoview.GeckoResponse;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;

import android.app.Activity;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.net.Uri;
import android.os.Bundle;
import android.view.Surface;

import java.util.HashMap;

public class TestRunnerActivity extends Activity {
    private static final String LOGTAG = "TestRunnerActivity";

    static GeckoRuntime sRuntime;

    private GeckoSession mSession;
    private GeckoView mView;
    private boolean mKillProcessOnDestroy;

    private HashMap<GeckoSession, GeckoDisplay> mDisplays = new HashMap<>();

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
        public void onLoadRequest(GeckoSession session, String uri, int target,
                                  int flags,
                                  GeckoResponse<Boolean> response) {
            // Allow Gecko to load all URIs
            response.respond(false);
        }

        @Override
        public void onNewSession(GeckoSession session, String uri, GeckoResponse<GeckoSession> response) {
            response.respond(createBackgroundSession(session.getSettings()));
        }
    };

    private GeckoSession.ContentDelegate mContentDelegate = new GeckoSession.ContentDelegate() {
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
        public void onContextMenu(GeckoSession session, int screenX, int screenY, String uri, int elementType, String elementSrc) {

        }

        @Override
        public void onExternalResponse(GeckoSession session, GeckoSession.WebResponseInfo request) {
        }

        @Override
        public void onCrash(GeckoSession session) {
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
        return session;
    }

    private GeckoSession createBackgroundSession(final GeckoSessionSettings settings) {
        final GeckoSession session = createSession(settings);

        final SurfaceTexture texture  = new SurfaceTexture(0);
        final Surface surface = new Surface(texture);

        final GeckoDisplay display = session.acquireDisplay();
        display.surfaceChanged(surface, mView.getWidth(), mView.getHeight());
        mDisplays.put(session, display);

        return session;
    }

    private void closeSession(GeckoSession session) {
        if (mDisplays.containsKey(session)) {
            final GeckoDisplay display = mDisplays.remove(session);
            display.surfaceDestroyed();

            session.releaseDisplay(display);
        }
        session.close();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Intent intent = getIntent();

        if (sRuntime == null) {
            final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                new GeckoRuntimeSettings.Builder();
            runtimeSettingsBuilder.arguments(new String[] { "-purgecaches" });
            final Bundle extras = intent.getExtras();
            if (extras != null) {
                runtimeSettingsBuilder.extras(extras);
            }

            runtimeSettingsBuilder
                    .nativeCrashReportingEnabled(true)
                    .javaCrashReportingEnabled(true)
                    .consoleOutput(true);

            sRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());
            sRuntime.setDelegate(new GeckoRuntime.Delegate() {
                @Override
                public void onShutdown() {
                    mKillProcessOnDestroy = true;
                    finish();
                }
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
