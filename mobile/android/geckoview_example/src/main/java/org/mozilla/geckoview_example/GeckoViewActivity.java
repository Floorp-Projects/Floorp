/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.WindowManager;

import java.util.Locale;

import org.mozilla.geckoview.GeckoResponse;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;

public class GeckoViewActivity extends Activity {
    private static final String LOGTAG = "GeckoViewActivity";
    private static final String DEFAULT_URL = "https://mozilla.org";
    private static final String USE_MULTIPROCESS_EXTRA = "use_multiprocess";
    private static final String USE_REMOTE_DEBUGGER_EXTRA = "use_remote_debugger";
    private static final String ACTION_SHUTDOWN =
        "org.mozilla.geckoview_example.SHUTDOWN";
    private boolean mKillProcessOnDestroy;

    /* package */ static final int REQUEST_FILE_PICKER = 1;
    private static final int REQUEST_PERMISSIONS = 2;

    private static GeckoRuntime sGeckoRuntime;
    private GeckoSession mGeckoSession;
    private GeckoView mGeckoView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
              " - application start");

        setContentView(R.layout.geckoview_activity);
        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);

        final boolean useMultiprocess =
            getIntent().getBooleanExtra(USE_MULTIPROCESS_EXTRA, true);

        if (sGeckoRuntime == null) {
            final GeckoRuntimeSettings.Builder runtimeSettingsBuilder =
                new GeckoRuntimeSettings.Builder();

            if (BuildConfig.DEBUG) {
                // In debug builds, we want to load JavaScript resources fresh with
                // each build.
                runtimeSettingsBuilder.arguments(new String[] { "-purgecaches" });
            }

            final Bundle extras = getIntent().getExtras();
            if (extras != null) {
                runtimeSettingsBuilder.extras(extras);
            }
            runtimeSettingsBuilder.useContentProcessHint(useMultiprocess);
            sGeckoRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());
        }

        final GeckoSessionSettings sessionSettings = new GeckoSessionSettings();
        sessionSettings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS,
                                   useMultiprocess);
        mGeckoSession = new GeckoSession(sessionSettings);

        mGeckoView.setSession(mGeckoSession, sGeckoRuntime);

        mGeckoSession.setContentDelegate(new MyGeckoViewContent());
        final MyTrackingProtection tp = new MyTrackingProtection();
        mGeckoSession.setTrackingProtectionDelegate(tp);
        mGeckoSession.setProgressDelegate(new MyGeckoViewProgress(tp));
        mGeckoSession.setNavigationDelegate(new Navigation());

        final BasicGeckoViewPrompt prompt = new BasicGeckoViewPrompt(this);
        prompt.filePickerRequestCode = REQUEST_FILE_PICKER;
        mGeckoSession.setPromptDelegate(prompt);

        final MyGeckoViewPermission permission = new MyGeckoViewPermission();
        permission.androidPermissionRequestCode = REQUEST_PERMISSIONS;
        mGeckoSession.setPermissionDelegate(permission);

        mGeckoSession.enableTrackingProtection(
              TrackingProtectionDelegate.CATEGORY_AD |
              TrackingProtectionDelegate.CATEGORY_ANALYTIC |
              TrackingProtectionDelegate.CATEGORY_SOCIAL
        );

        loadSettings(getIntent());
        loadFromIntent(getIntent());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mKillProcessOnDestroy) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }

    @Override
    protected void onNewIntent(final Intent intent) {
        super.onNewIntent(intent);

        if (ACTION_SHUTDOWN.equals(intent.getAction())) {
            mKillProcessOnDestroy = true;
            if (sGeckoRuntime != null) {
                sGeckoRuntime.shutdown();
            }
            finish();
            return;
        }

        setIntent(intent);

        loadSettings(intent);
        if (intent.getData() != null) {
            loadFromIntent(intent);
        }
    }

    private void loadFromIntent(final Intent intent) {
        final Uri uri = intent.getData();
        mGeckoSession.loadUri(uri != null ? uri.toString() : DEFAULT_URL);
    }

    private void loadSettings(final Intent intent) {
        sGeckoRuntime.getSettings().setRemoteDebuggingEnabled(
            intent.getBooleanExtra(USE_REMOTE_DEBUGGER_EXTRA, false));
    }

    @Override
    protected void onActivityResult(final int requestCode, final int resultCode,
                                    final Intent data) {
        if (requestCode == REQUEST_FILE_PICKER) {
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoSession.getPromptDelegate();
            prompt.onFileCallbackResult(resultCode, data);
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void onRequestPermissionsResult(final int requestCode,
                                           final String[] permissions,
                                           final int[] grantResults) {
        if (requestCode == REQUEST_PERMISSIONS) {
            final MyGeckoViewPermission permission = (MyGeckoViewPermission)
                    mGeckoSession.getPermissionDelegate();
            permission.onRequestPermissionsResult(permissions, grantResults);
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    private class MyGeckoViewContent implements GeckoSession.ContentDelegate {
        @Override
        public void onTitleChange(GeckoSession session, String title) {
            Log.i(LOGTAG, "Content title changed to " + title);
        }

        @Override
        public void onFullScreen(final GeckoSession session, final boolean fullScreen) {
            getWindow().setFlags(fullScreen ? WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                                 WindowManager.LayoutParams.FLAG_FULLSCREEN);
            if (fullScreen) {
                getActionBar().hide();
            } else {
                getActionBar().show();
            }
        }

        @Override
        public void onFocusRequest(final GeckoSession session) {
            Log.i(LOGTAG, "Content requesting focus");
        }

        @Override
        public void onCloseRequest(final GeckoSession session) {
            if (session != mGeckoSession) {
                session.close();
            }
        }

        @Override
        public void onContextMenu(GeckoSession session, int screenX, int screenY,
                                  String uri, int elementType, String elementSrc) {
            Log.d(LOGTAG, "onContextMenu screenX=" + screenX +
                          " screenY=" + screenY + " uri=" + uri +
                          " elementType=" + elementType +
                          " elementSrc=" + elementSrc);
        }

        @Override
        public void onExternalResponse(GeckoSession session, GeckoSession.WebResponseInfo request) {
        }
    }

    private class MyGeckoViewProgress implements GeckoSession.ProgressDelegate {
        private MyTrackingProtection mTp;

        private MyGeckoViewProgress(final MyTrackingProtection tp) {
            mTp = tp;
        }

        @Override
        public void onPageStart(GeckoSession session, String url) {
            Log.i(LOGTAG, "Starting to load page at " + url);
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load start");
            mTp.clearCounters();
        }

        @Override
        public void onPageStop(GeckoSession session, boolean success) {
            Log.i(LOGTAG, "Stopping page load " + (success ? "successfully" : "unsuccessfully"));
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load stop");
            mTp.logCounters();
        }

        @Override
        public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {
            Log.i(LOGTAG, "Security status changed to " + securityInfo.securityMode);
        }
    }

    private class MyGeckoViewPermission implements GeckoSession.PermissionDelegate {

        public int androidPermissionRequestCode = 1;
        private Callback mCallback;

        public void onRequestPermissionsResult(final String[] permissions,
                                               final int[] grantResults) {
            if (mCallback == null) {
                return;
            }

            final Callback cb = mCallback;
            mCallback = null;
            for (final int result : grantResults) {
                if (result != PackageManager.PERMISSION_GRANTED) {
                    // At least one permission was not granted.
                    cb.reject();
                    return;
                }
            }
            cb.grant();
        }

        @Override
        public void onAndroidPermissionsRequest(final GeckoSession session, final String[] permissions,
                                              final Callback callback) {
            if (Build.VERSION.SDK_INT < 23) {
                // requestPermissions was introduced in API 23.
                callback.grant();
                return;
            }
            mCallback = callback;
            requestPermissions(permissions, androidPermissionRequestCode);
        }

        @Override
        public void onContentPermissionRequest(final GeckoSession session, final String uri,
                                             final int type, final String access,
                                             final Callback callback) {
            final int resId;
            if (PERMISSION_GEOLOCATION == type) {
                resId = R.string.request_geolocation;
            } else if (PERMISSION_DESKTOP_NOTIFICATION == type) {
                resId = R.string.request_notification;
            } else {
                Log.w(LOGTAG, "Unknown permission: " + type);
                callback.reject();
                return;
            }

            final String title = getString(resId, Uri.parse(uri).getAuthority());
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoSession.getPromptDelegate();
            prompt.onPermissionPrompt(session, title, callback);
        }

        private String[] normalizeMediaName(final MediaSource[] sources) {
            if (sources == null) {
                return null;
            }

            String[] res = new String[sources.length];
            for (int i = 0; i < sources.length; i++) {
                final int mediaSource = sources[i].source;
                final String name = sources[i].name;
                if (MediaSource.SOURCE_CAMERA == mediaSource) {
                    if (name.toLowerCase(Locale.ENGLISH).contains("front")) {
                        res[i] = getString(R.string.media_front_camera);
                    } else {
                        res[i] = getString(R.string.media_back_camera);
                    }
                } else if (!name.isEmpty()) {
                    res[i] = name;
                } else if (MediaSource.SOURCE_MICROPHONE == mediaSource) {
                    res[i] = getString(R.string.media_microphone);
                } else {
                    res[i] = getString(R.string.media_other);
                }
            }

            return res;
        }

        @Override
        public void onMediaPermissionRequest(final GeckoSession session, final String uri,
                                           final MediaSource[] video, final MediaSource[] audio,
                                           final MediaCallback callback) {
            final String host = Uri.parse(uri).getAuthority();
            final String title;
            if (audio == null) {
                title = getString(R.string.request_video, host);
            } else if (video == null) {
                title = getString(R.string.request_audio, host);
            } else {
                title = getString(R.string.request_media, host);
            }

            String[] videoNames = normalizeMediaName(video);
            String[] audioNames = normalizeMediaName(audio);

            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoSession.getPromptDelegate();
            prompt.onMediaPrompt(session, title, video, audio, videoNames, audioNames, callback);
        }
    }

    private class Navigation implements GeckoSession.NavigationDelegate {
        @Override
        public void onLocationChange(GeckoSession session, final String url) {
        }

        @Override
        public void onCanGoBack(GeckoSession session, boolean canGoBack) {
        }

        @Override
        public void onCanGoForward(GeckoSession session, boolean value) {
        }

        @Override
        public void onLoadRequest(final GeckoSession session, final String uri,
                                  final int target, final int flags,
                                  GeckoResponse<Boolean> response) {
            Log.d(LOGTAG, "onLoadRequest=" + uri + " where=" + target +
                  " flags=" + flags);
            response.respond(false);
        }

        @Override
        public void onNewSession(final GeckoSession session, final String uri, GeckoResponse<GeckoSession> response) {
            response.respond(null);
        }
    }

    private class MyTrackingProtection implements GeckoSession.TrackingProtectionDelegate {
        private int mBlockedAds = 0;
        private int mBlockedAnalytics = 0;
        private int mBlockedSocial = 0;

        private void clearCounters() {
            mBlockedAds = 0;
            mBlockedAnalytics = 0;
            mBlockedSocial = 0;
        }

        private void logCounters() {
            Log.d(LOGTAG, "Trackers blocked: " + mBlockedAds + " ads, " +
                  mBlockedAnalytics + " analytics, " +
                  mBlockedSocial + " social");
        }

        @Override
        public void onTrackerBlocked(final GeckoSession session, final String uri,
                                     int categories) {
            Log.d(LOGTAG, "onTrackerBlocked " + categories + " (" + uri + ")");
            mBlockedAds += categories & TrackingProtectionDelegate.CATEGORY_AD;
            mBlockedAnalytics += categories & TrackingProtectionDelegate.CATEGORY_ANALYTIC;
            mBlockedSocial += categories & TrackingProtectionDelegate.CATEGORY_SOCIAL;
        }
    }
}
