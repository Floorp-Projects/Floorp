/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test;

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
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import android.view.Surface;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class TestRunnerActivity extends Activity {
    private static final String LOGTAG = "TestRunnerActivity";
    private static final String ERROR_PAGE =
            "<!DOCTYPE html><head><title>Error</title></head><body>Error!</body></html>";

    static GeckoRuntime sRuntime;

    private GeckoSession mPopupSession;
    private GeckoSession mSession;
    private GeckoView mView;
    private boolean mKillProcessOnDestroy;

    private HashMap<GeckoSession, Display> mDisplays = new HashMap<>();
    private List<WebExtension> mExtensions = new ArrayList<>();

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

    private static WebExtensionController webExtensionController() {
        return sRuntime.getWebExtensionController();
    }

    // Keeps track of all sessions for this test runner. The top session in the deque is the
    // current active session for extension purposes.
    private ArrayDeque<GeckoSession> mOwnedSessions = new ArrayDeque<>();

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
        public GeckoResult<AllowOrDeny> onLoadRequest(GeckoSession session,
                                                  LoadRequest request) {
            // Allow Gecko to load all URIs
            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }

        @Override
        public GeckoResult<GeckoSession> onNewSession(GeckoSession session, String uri) {
            webExtensionController().setTabActive(mOwnedSessions.peek(), false);
            GeckoSession newSession = createBackgroundSession(session.getSettings(),
                                                              /* active */ true);
            webExtensionController().setTabActive(newSession, true);
            return GeckoResult.fromValue(newSession);
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
        public void onCloseRequest(GeckoSession session) {
            closeSession(session);
        }

        @Override
        public void onCrash(GeckoSession session) {
            onContentProcessGone();
        }

        @Override
        public void onKill(GeckoSession session) {
            onContentProcessGone();
        }
    };

    private WebExtension.ActionDelegate mActionDelegate = new WebExtension.ActionDelegate() {
        @Nullable
        @Override
        public GeckoResult<GeckoSession> onOpenPopup(@NonNull WebExtension extension,
                                                     @NonNull WebExtension.Action action) {
            if (mPopupSession != null) {
                mPopupSession.close();
            }

            mPopupSession = createBackgroundSession(null, /* active */ false);
            mPopupSession.open(sRuntime);

            return GeckoResult.fromValue(mPopupSession);
        }
    };

    private WebExtension.SessionTabDelegate mSessionTabDelegate = new WebExtension.SessionTabDelegate() {
        @NonNull
        @Override
        public GeckoResult<AllowOrDeny> onCloseTab(@Nullable WebExtension source,
                                                   @NonNull GeckoSession session) {
            closeSession(session);
            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }
        @Override
        public GeckoResult<AllowOrDeny> onUpdateTab(@NonNull WebExtension source,
                                                    @NonNull GeckoSession session,
                                                    @NonNull WebExtension.UpdateTabDetails updateDetails) {
            if (updateDetails.active == Boolean.TRUE) {
                // Move session to the top since it's now the active tab
                mOwnedSessions.remove(session);
                mOwnedSessions.addFirst(session);
            }

            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }
    };

    /**
     * Creates a session and adds it to the owned sessions deque.
     *
     * @param active Whether this session is the "active" session for extension purposes.
     *               The active session always sit at the top of the owned sessions deque.
     * @return the newly created session.
     */
    private GeckoSession createSession(boolean active) {
        return createSession(null, active);
    }

    /**
     * Creates a session and adds it to the owned sessions deque.
     *
     * @param settings settings for the newly created {@link GeckoSession}, could be null
     *                 if no extra settings need to be added.
     * @param active Whether this session is the "active" session for extension purposes.
     *               The active session always sit at the top of the owned sessions deque.
     * @return the newly created session.
     */
    private GeckoSession createSession(GeckoSessionSettings settings, boolean active) {
        if (settings == null) {
            settings = new GeckoSessionSettings();
        }

        final GeckoSession session = new GeckoSession(settings);
        session.setNavigationDelegate(mNavigationDelegate);
        session.setContentDelegate(mContentDelegate);
        session.setPermissionDelegate(mPermissionDelegate);

        final WebExtension.SessionController sessionController =
                session.getWebExtensionController();
        for (final WebExtension extension : mExtensions) {
            sessionController.setActionDelegate(extension, mActionDelegate);
            sessionController.setTabDelegate(extension, mSessionTabDelegate);
        }

        if (active) {
            mOwnedSessions.addFirst(session);
        } else {
            mOwnedSessions.addLast(session);
        }
        return session;
    }

    /**
     * Creates a session with a display attached.
     *
     * @param settings settings for the newly created {@link GeckoSession}, could be null
     *                 if no extra settings need to be added.
     * @param active Whether this session is the "active" session for extension purposes.
     *               The active session always sit at the top of the owned sessions deque.
     * @return the newly created session.
     */
    private GeckoSession createBackgroundSession(final GeckoSessionSettings settings, boolean active) {
        final GeckoSession session = createSession(settings, active);

        final Display display = new Display(mView.getWidth(), mView.getHeight());
        display.attach(session);

        mDisplays.put(session, display);

        return session;
    }

    private void closeSession(GeckoSession session) {
        if (session == mOwnedSessions.peek()) {
            webExtensionController().setTabActive(session, false);
        }
        if (mDisplays.containsKey(session)) {
            final Display display = mDisplays.remove(session);
            display.release(session);
        }
        mOwnedSessions.remove(session);
        session.close();
        if (!mOwnedSessions.isEmpty()) {
            // Pick the top session as the current active
            webExtensionController().setTabActive(mOwnedSessions.peek(), true);
        }
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
                    .remoteDebuggingEnabled(true);

            final Bundle extras = intent.getExtras();
            if (extras != null) {
                runtimeSettingsBuilder.extras(extras);
            }

            runtimeSettingsBuilder
                    .consoleOutput(true)
                    .crashHandler(TestCrashHandler.class);

            sRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());

            webExtensionController().setDebuggerDelegate(new WebExtensionController.DebuggerDelegate() {
                @Override
                public void onExtensionListUpdated() {
                    refreshExtensionList();
                }
            });

            sRuntime.setDelegate(() -> {
                mKillProcessOnDestroy = true;
                finish();
            });
        }

        mSession = createSession(/* active */ true);
        webExtensionController().setTabActive(mOwnedSessions.peek(), true);
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

    private void refreshExtensionList() {
        webExtensionController().list().accept(extensions -> {
            mExtensions = extensions;
            for (WebExtension extension : mExtensions) {
                extension.setActionDelegate(mActionDelegate);
                extension.setTabDelegate(new WebExtension.TabDelegate() {
                    @Override
                    public GeckoResult<GeckoSession> onNewTab(WebExtension source,
                                                              WebExtension.CreateTabDetails details) {
                        GeckoSessionSettings settings = null;
                        if (details.cookieStoreId != null) {
                            settings = new GeckoSessionSettings.Builder()
                                    .contextId(details.cookieStoreId)
                                    .build();
                        }

                        if (details.active == Boolean.TRUE) {
                            webExtensionController().setTabActive(mOwnedSessions.peek(), false);
                        }
                        GeckoSession newSession = createSession(
                                settings,
                                details.active == Boolean.TRUE);
                        return GeckoResult.fromValue(newSession);
                    }
                });

                for (final GeckoSession session : mOwnedSessions) {
                    final WebExtension.SessionController controller =
                            session.getWebExtensionController();
                    controller.setActionDelegate(extension, mActionDelegate);
                    controller.setTabDelegate(extension, mSessionTabDelegate);
                }
            }
        });
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
