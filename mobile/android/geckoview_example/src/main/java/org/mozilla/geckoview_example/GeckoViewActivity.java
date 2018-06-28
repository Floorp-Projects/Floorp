/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import org.mozilla.geckoview.BasicSelectionActionDelegate;
import org.mozilla.geckoview.GeckoResponse;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;

import android.Manifest;
import android.app.DownloadManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.SystemClock;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.WindowManager;

import java.util.LinkedList;
import java.util.Locale;

public class GeckoViewActivity extends AppCompatActivity {
    private static final String LOGTAG = "GeckoViewActivity";
    private static final String DEFAULT_URL = "https://mozilla.org";
    private static final String USE_MULTIPROCESS_EXTRA = "use_multiprocess";
    private static final String SEARCH_URI_BASE = "https://www.google.com/search?q=";
    private static final String ACTION_SHUTDOWN = "org.mozilla.geckoview_example.SHUTDOWN";
    private static final int REQUEST_FILE_PICKER = 1;
    private static final int REQUEST_PERMISSIONS = 2;
    private static final int REQUEST_WRITE_EXTERNAL_STORAGE = 3;

    private static GeckoRuntime sGeckoRuntime;
    private GeckoSession mGeckoSession;
    private GeckoView mGeckoView;
    private boolean mUseMultiprocess;
    private boolean mUseTrackingProtection;
    private boolean mUsePrivateBrowsing;
    private boolean mKillProcessOnDestroy;

    private LocationView mLocationView;
    private String mCurrentUri;
    private boolean mCanGoBack;
    private boolean mCanGoForward;
    private boolean mFullScreen;

    private LinkedList<GeckoSession.WebResponseInfo> mPendingDownloads = new LinkedList<>();

    private LocationView.CommitListener mCommitListener = new LocationView.CommitListener() {
        @Override
        public void onCommit(String text) {
            if ((text.contains(".") || text.contains(":")) && !text.contains(" ")) {
                mGeckoSession.loadUri(text);
            } else {
                mGeckoSession.loadUri(SEARCH_URI_BASE + text);
            }
            mGeckoView.requestFocus();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
              " - application start");

        setContentView(R.layout.geckoview_activity);
        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);

        setSupportActionBar((Toolbar)findViewById(R.id.toolbar));

        mLocationView = new LocationView(this);
        getSupportActionBar().setCustomView(mLocationView,
                new ActionBar.LayoutParams(ActionBar.LayoutParams.MATCH_PARENT,
                        ActionBar.LayoutParams.WRAP_CONTENT));
        getSupportActionBar().setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM);

        mUseMultiprocess = getIntent().getBooleanExtra(USE_MULTIPROCESS_EXTRA, true);

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
            runtimeSettingsBuilder
                    .useContentProcessHint(mUseMultiprocess)
                    .remoteDebuggingEnabled(true)
                    .nativeCrashReportingEnabled(true)
                    .javaCrashReportingEnabled(true)
                    .consoleOutput(true)
                    .trackingProtectionCategories(TrackingProtectionDelegate.CATEGORY_ALL);

            sGeckoRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());
        }

        mGeckoSession = (GeckoSession)getIntent().getParcelableExtra("session");
        if (mGeckoSession != null) {
            connectSession(mGeckoSession);

            if (!mGeckoSession.isOpen()) {
                mGeckoSession.open(sGeckoRuntime);
            }

            mUseMultiprocess = mGeckoSession.getSettings().getBoolean(GeckoSessionSettings.USE_MULTIPROCESS);

            mGeckoView.setSession(mGeckoSession);
        } else {
            mGeckoSession = createSession();
            mGeckoView.setSession(mGeckoSession, sGeckoRuntime);
            loadFromIntent(getIntent());
        }

        mLocationView.setCommitListener(mCommitListener);
    }

    private GeckoSession createSession() {
        GeckoSession session = new GeckoSession();
        session.getSettings().setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, mUseMultiprocess);
        session.getSettings().setBoolean(GeckoSessionSettings.USE_PRIVATE_MODE, mUsePrivateBrowsing);
        session.getSettings().setBoolean(
            GeckoSessionSettings.USE_TRACKING_PROTECTION, mUseTrackingProtection);

        connectSession(session);

        return session;
    }

    private void connectSession(GeckoSession session) {
        session.setContentDelegate(new ExampleContentDelegate());
        final ExampleTrackingProtectionDelegate tp = new ExampleTrackingProtectionDelegate();
        session.setTrackingProtectionDelegate(tp);
        session.setProgressDelegate(new ExampleProgressDelegate(tp));
        session.setNavigationDelegate(new ExampleNavigationDelegate());

        final BasicGeckoViewPrompt prompt = new BasicGeckoViewPrompt(this);
        prompt.filePickerRequestCode = REQUEST_FILE_PICKER;
        session.setPromptDelegate(prompt);

        final ExamplePermissionDelegate permission = new ExamplePermissionDelegate();
        permission.androidPermissionRequestCode = REQUEST_PERMISSIONS;
        session.setPermissionDelegate(permission);

        session.setSelectionActionDelegate(new BasicSelectionActionDelegate(this));

        updateTrackingProtection(session);
    }

    private void recreateSession() {
        mGeckoSession.close();

        mGeckoSession = createSession();
        mGeckoSession.open(sGeckoRuntime);
        mGeckoView.setSession(mGeckoSession);
        mGeckoSession.loadUri(mCurrentUri != null ? mCurrentUri : DEFAULT_URL);
    }

    private void updateTrackingProtection(GeckoSession session) {
        session.getSettings().setBoolean(
            GeckoSessionSettings.USE_TRACKING_PROTECTION, mUseTrackingProtection);
    }

    @Override
    protected void onPause() {
        mGeckoSession.setActive(false);
        super.onPause();
    }

    @Override
    protected void onResume() {
        mGeckoSession.setActive(true);
        super.onResume();
    }

    @Override
    public void onBackPressed() {
        if (mFullScreen) {
            mGeckoSession.exitFullScreen();
            return;
        }

        if (mCanGoBack && mGeckoSession != null) {
            mGeckoSession.goBack();
            return;
        }

        super.onBackPressed();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.actions, menu);
        return true;
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        menu.findItem(R.id.action_e10s).setChecked(mUseMultiprocess);
        menu.findItem(R.id.action_tp).setChecked(mUseTrackingProtection);
        menu.findItem(R.id.action_pb).setChecked(mUsePrivateBrowsing);
        menu.findItem(R.id.action_forward).setEnabled(mCanGoForward);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.action_reload:
                mGeckoSession.reload();
                break;
            case R.id.action_forward:
                mGeckoSession.goForward();
                break;
            case R.id.action_e10s:
                mUseMultiprocess = !mUseMultiprocess;
                recreateSession();
                break;
            case R.id.action_tp:
                mUseTrackingProtection = !mUseTrackingProtection;
                updateTrackingProtection(mGeckoSession);
                mGeckoSession.reload();
                break;
            case R.id.action_pb:
                mUsePrivateBrowsing = !mUsePrivateBrowsing;
                recreateSession();
                break;
            default:
                return super.onOptionsItemSelected(item);
        }

        return true;
    }

    @Override
    public void onDestroy() {
        if (mKillProcessOnDestroy) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }

        super.onDestroy();
    }

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

        if (intent.getData() != null) {
            loadFromIntent(intent);
        }
    }


        private void loadFromIntent(final Intent intent) {
        final Uri uri = intent.getData();
        mGeckoSession.loadUri(uri != null ? uri.toString() : DEFAULT_URL);
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
            final ExamplePermissionDelegate permission = (ExamplePermissionDelegate)
                    mGeckoSession.getPermissionDelegate();
            permission.onRequestPermissionsResult(permissions, grantResults);
        } else if (requestCode == REQUEST_WRITE_EXTERNAL_STORAGE &&
                   grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            continueDownloads();
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    private void continueDownloads() {
        LinkedList<GeckoSession.WebResponseInfo> downloads = mPendingDownloads;
        mPendingDownloads = new LinkedList<>();

        for (GeckoSession.WebResponseInfo response : downloads) {
            downloadFile(response);
        }
    }

    private void downloadFile(GeckoSession.WebResponseInfo response) {
        if (ContextCompat.checkSelfPermission(GeckoViewActivity.this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            mPendingDownloads.add(response);
            ActivityCompat.requestPermissions(GeckoViewActivity.this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    REQUEST_WRITE_EXTERNAL_STORAGE);
            return;
        }

        final Uri uri = Uri.parse(response.uri);
        final String filename = response.filename != null ? response.filename : uri.getLastPathSegment();

        DownloadManager manager = (DownloadManager) getSystemService(DOWNLOAD_SERVICE);
        DownloadManager.Request req = new DownloadManager.Request(uri);
        req.setMimeType(response.contentType);
        req.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, filename);
        req.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE);
        manager.enqueue(req);
    }

    private class ExampleContentDelegate implements GeckoSession.ContentDelegate {
        @Override
        public void onTitleChange(GeckoSession session, String title) {
            Log.i(LOGTAG, "Content title changed to " + title);
        }

        @Override
        public void onFullScreen(final GeckoSession session, final boolean fullScreen) {
            getWindow().setFlags(fullScreen ? WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                                 WindowManager.LayoutParams.FLAG_FULLSCREEN);
            mFullScreen = fullScreen;
            if (fullScreen) {
                getSupportActionBar().hide();
            } else {
                getSupportActionBar().show();
            }
        }

        @Override
        public void onFocusRequest(final GeckoSession session) {
            Log.i(LOGTAG, "Content requesting focus");
        }

        @Override
        public void onCloseRequest(final GeckoSession session) {
            if (session == mGeckoSession) {
                finish();
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
        public void onExternalResponse(GeckoSession session, GeckoSession.WebResponseInfo response) {
            try {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setDataAndTypeAndNormalize(Uri.parse(response.uri), response.contentType);
                startActivity(intent);
            } catch (ActivityNotFoundException e) {
                downloadFile(response);
            }
        }

        @Override
        public void onCrash(GeckoSession session) {
            Log.e(LOGTAG, "Crashed, reopening session");
            session.open(sGeckoRuntime);
            session.loadUri(DEFAULT_URL);
        }
    }

    private class ExampleProgressDelegate implements GeckoSession.ProgressDelegate {
        private ExampleTrackingProtectionDelegate mTp;

        private ExampleProgressDelegate(final ExampleTrackingProtectionDelegate tp) {
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

    private class ExamplePermissionDelegate implements GeckoSession.PermissionDelegate {

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
            if (Build.VERSION.SDK_INT >= 23) {
                // requestPermissions was introduced in API 23.
                mCallback = callback;
                requestPermissions(permissions, androidPermissionRequestCode);
            } else {
                callback.grant();
            }
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

    private class ExampleNavigationDelegate implements GeckoSession.NavigationDelegate {
        @Override
        public void onLocationChange(GeckoSession session, final String url) {
            mLocationView.setText(url);
            mCurrentUri = url;
        }

        @Override
        public void onCanGoBack(GeckoSession session, boolean canGoBack) {
            mCanGoBack = canGoBack;
        }

        @Override
        public void onCanGoForward(GeckoSession session, boolean canGoForward) {
            mCanGoForward = canGoForward;
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
            GeckoSession newSession = new GeckoSession(session.getSettings());
            response.respond(newSession);

            Intent intent = new Intent(GeckoViewActivity.this, SessionActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
            intent.setAction(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(uri));
            intent.putExtra("session", newSession);

            startActivity(intent);
        }
    }

    private class ExampleTrackingProtectionDelegate implements GeckoSession.TrackingProtectionDelegate {
        private int mBlockedAds = 0;
        private int mBlockedAnalytics = 0;
        private int mBlockedSocial = 0;
        private int mBlockedContent = 0;
        private int mBlockedTest = 0;

        private void clearCounters() {
            mBlockedAds = 0;
            mBlockedAnalytics = 0;
            mBlockedSocial = 0;
            mBlockedContent = 0;
            mBlockedTest = 0;
        }

        private void logCounters() {
            Log.d(LOGTAG, "Trackers blocked: " + mBlockedAds + " ads, " +
                  mBlockedAnalytics + " analytics, " +
                  mBlockedSocial + " social, " +
                  mBlockedContent + " content, " +
                  mBlockedTest + " test");
        }

        @Override
        public void onTrackerBlocked(final GeckoSession session, final String uri,
                                     int categories) {
            Log.d(LOGTAG, "onTrackerBlocked " + categories + " (" + uri + ")");
            if ((categories & TrackingProtectionDelegate.CATEGORY_TEST) != 0) {
                mBlockedTest++;
            }
            if ((categories & TrackingProtectionDelegate.CATEGORY_AD) != 0) {
                mBlockedAds++;
            }
            if ((categories & TrackingProtectionDelegate.CATEGORY_ANALYTIC) != 0) {
                mBlockedAnalytics++;
            }
            if ((categories & TrackingProtectionDelegate.CATEGORY_SOCIAL) != 0) {
                mBlockedSocial++;
            }
            if ((categories & TrackingProtectionDelegate.CATEGORY_CONTENT) != 0) {
                mBlockedContent++;
            }
        }
    }
}
