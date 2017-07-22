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

import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoViewSettings;
import org.mozilla.gecko.util.GeckoBundle;

public class GeckoViewActivity extends Activity {
    private static final String LOGTAG = "GeckoViewActivity";
    private static final String DEFAULT_URL = "https://mozilla.org";
    private static final String USE_MULTIPROCESS_EXTRA = "use_multiprocess";

    /* package */ static final int REQUEST_FILE_PICKER = 1;
    private static final int REQUEST_PERMISSIONS = 2;

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

        GeckoView.preload(this, geckoArgs);

        setContentView(R.layout.geckoview_activity);

        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);
        mGeckoView.setContentListener(new MyGeckoViewContent());
        mGeckoView.setProgressListener(new MyGeckoViewProgress());

        final BasicGeckoViewPrompt prompt = new BasicGeckoViewPrompt();
        prompt.filePickerRequestCode = REQUEST_FILE_PICKER;
        mGeckoView.setPromptDelegate(prompt);

        final MyGeckoViewPermission permission = new MyGeckoViewPermission();
        permission.androidPermissionRequestCode = REQUEST_PERMISSIONS;
        mGeckoView.setPermissionDelegate(permission);

        loadFromIntent(getIntent());
    }

    @Override
    protected void onNewIntent(final Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);

        if (intent.getData() != null) {
            loadFromIntent(intent);
        }
    }

    private void loadFromIntent(final Intent intent) {
        mGeckoView.getSettings().setBoolean(
            GeckoViewSettings.USE_MULTIPROCESS,
            intent.getBooleanExtra(USE_MULTIPROCESS_EXTRA, true));

        final Uri uri = intent.getData();
        mGeckoView.loadUri(uri != null ? uri.toString() : DEFAULT_URL);
    }

    @Override
    protected void onActivityResult(final int requestCode, final int resultCode,
                                    final Intent data) {
        if (requestCode == REQUEST_FILE_PICKER) {
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoView.getPromptDelegate();
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
                    mGeckoView.getPermissionDelegate();
            permission.onRequestPermissionsResult(permissions, grantResults);
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    private class MyGeckoViewContent implements GeckoView.ContentListener {
        @Override
        public void onTitleChange(GeckoView view, String title) {
            Log.i(LOGTAG, "Content title changed to " + title);
        }

        @Override
        public void onFullScreen(final GeckoView view, final boolean fullScreen) {
            getWindow().setFlags(fullScreen ? WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                                 WindowManager.LayoutParams.FLAG_FULLSCREEN);
            if (fullScreen) {
                getActionBar().hide();
            } else {
                getActionBar().show();
            }
        }
    }

    private class MyGeckoViewProgress implements GeckoView.ProgressListener {
        @Override
        public void onPageStart(GeckoView view, String url) {
            Log.i(LOGTAG, "Starting to load page at " + url);
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load start");
        }

        @Override
        public void onPageStop(GeckoView view, boolean success) {
            Log.i(LOGTAG, "Stopping page load " + (success ? "successfully" : "unsuccessfully"));
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load stop");
        }

        @Override
        public void onSecurityChange(GeckoView view, int status, GeckoBundle identity) {
            String statusString;
            if ((status & STATE_IS_BROKEN) != 0) {
                statusString = "broken";
            } else if ((status & STATE_IS_SECURE) != 0) {
                statusString = "secure";
            } else if ((status & STATE_IS_INSECURE) != 0) {
                statusString = "insecure";
            } else {
                statusString = "unknown";
            }
            Log.i(LOGTAG, "Security status changed to " + statusString);
        }
    }

    private class MyGeckoViewPermission implements GeckoView.PermissionDelegate {

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
        public void requestAndroidPermissions(final GeckoView view, final String[] permissions,
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
        public void requestContentPermission(final GeckoView view, final String uri,
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
                    mGeckoView.getPromptDelegate();
            prompt.promptForPermission(view, title, callback);
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
        public void requestMediaPermission(final GeckoView view, final String uri,
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
                    mGeckoView.getPromptDelegate();
            prompt.promptForMedia(view, title, video, audio, callback);
        }
    }
}
