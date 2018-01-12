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
import android.text.TextUtils;
import android.util.Log;
import android.view.WindowManager;

import java.util.Locale;

import org.mozilla.gecko.GeckoSession;
import org.mozilla.gecko.GeckoSessionSettings;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.util.GeckoBundle;

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

    private GeckoSession mGeckoSession;
    private GeckoView mGeckoView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
              " - application start");

        String geckoArgs = null;
        final String intentArgs = getIntent().getStringExtra("args");

        if (BuildConfig.DEBUG) {
            // In debug builds, we want to load JavaScript resources fresh with each build.
            geckoArgs = "-purgecaches";
        }

        if (!TextUtils.isEmpty(intentArgs)) {
            if (geckoArgs == null) {
                geckoArgs = intentArgs;
            } else {
                geckoArgs += " " + intentArgs;
            }
        }

        final boolean useMultiprocess = getIntent().getBooleanExtra(USE_MULTIPROCESS_EXTRA,
                                                                    true);
        GeckoSession.preload(this, geckoArgs, useMultiprocess);

        setContentView(R.layout.geckoview_activity);

        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);
        mGeckoSession = new GeckoSession();
        mGeckoView.setSession(mGeckoSession);

        mGeckoSession.setContentListener(new MyGeckoViewContent());
        mGeckoSession.setProgressListener(new MyGeckoViewProgress());
        mGeckoSession.setNavigationListener(new Navigation());

        final BasicGeckoViewPrompt prompt = new BasicGeckoViewPrompt(this);
        prompt.filePickerRequestCode = REQUEST_FILE_PICKER;
        mGeckoSession.setPromptDelegate(prompt);

        final MyGeckoViewPermission permission = new MyGeckoViewPermission();
        permission.androidPermissionRequestCode = REQUEST_PERMISSIONS;
        mGeckoSession.setPermissionDelegate(permission);

        mGeckoView.getSettings().setBoolean(GeckoSessionSettings.USE_MULTIPROCESS,
                                            useMultiprocess);
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
            GeckoThread.forceQuit();
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
        final GeckoSessionSettings settings = mGeckoView.getSettings();
        settings.setBoolean(
            GeckoSessionSettings.USE_REMOTE_DEBUGGER,
            intent.getBooleanExtra(USE_REMOTE_DEBUGGER_EXTRA, false));

        Log.i(LOGTAG, "Load with settings " + settings);
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

    private class MyGeckoViewContent implements GeckoSession.ContentListener {
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
        public void onContextMenu(GeckoSession session, int screenX, int screenY,
                                  String uri, String elementSrc) {
            Log.d(LOGTAG, "onContextMenu screenX=" + screenX +
                          " screenY=" + screenY + " uri=" + uri +
                          " elementSrc=" + elementSrc);
        }
    }

    private class MyGeckoViewProgress implements GeckoSession.ProgressListener {
        @Override
        public void onPageStart(GeckoSession session, String url) {
            Log.i(LOGTAG, "Starting to load page at " + url);
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load start");
        }

        @Override
        public void onPageStop(GeckoSession session, boolean success) {
            Log.i(LOGTAG, "Stopping page load " + (success ? "successfully" : "unsuccessfully"));
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load stop");
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
        public void requestAndroidPermissions(final GeckoSession session, final String[] permissions,
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
        public void requestContentPermission(final GeckoSession session, final String uri,
                                             final String type, final String access,
                                             final Callback callback) {
            final int resId;
            if ("geolocation".equals(type)) {
                resId = R.string.request_geolocation;
            } else if ("desktop-notification".equals(type)) {
                resId = R.string.request_notification;
            } else {
                Log.w(LOGTAG, "Unknown permission: " + type);
                callback.reject();
                return;
            }

            final String title = getString(resId, Uri.parse(uri).getAuthority());
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoSession.getPromptDelegate();
            prompt.promptForPermission(session, title, callback);
        }

        private void normalizeMediaName(final GeckoBundle[] sources) {
            if (sources == null) {
                return;
            }
            for (final GeckoBundle source : sources) {
                final String mediaSource = source.getString("mediaSource");
                String name = source.getString("name");
                if ("camera".equals(mediaSource)) {
                    if (name.toLowerCase(Locale.ENGLISH).contains("front")) {
                        name = getString(R.string.media_front_camera);
                    } else {
                        name = getString(R.string.media_back_camera);
                    }
                } else if (!name.isEmpty()) {
                    continue;
                } else if ("microphone".equals(mediaSource)) {
                    name = getString(R.string.media_microphone);
                } else {
                    name = getString(R.string.media_other);
                }
                source.putString("name", name);
            }
        }

        @Override
        public void requestMediaPermission(final GeckoSession session, final String uri,
                                           final GeckoBundle[] video,
                                           final GeckoBundle[] audio,
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

            normalizeMediaName(video);
            normalizeMediaName(audio);

            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoSession.getPromptDelegate();
            prompt.promptForMedia(session, title, video, audio, callback);
        }
    }

    private class Navigation implements GeckoSession.NavigationListener {
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
        public boolean onLoadUri(final GeckoSession session, final String uri,
                                 final TargetWindow where) {
            Log.d(LOGTAG, "onLoadUri=" + uri +
                          " where=" + where);
            if (where != TargetWindow.NEW) {
                return false;
            }
            session.loadUri(uri);
            return true;
        }
    }
}
