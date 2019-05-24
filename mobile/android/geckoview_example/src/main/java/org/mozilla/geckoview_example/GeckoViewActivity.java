/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import org.json.JSONObject;
import org.mozilla.geckoview.AllowOrDeny;
import org.mozilla.geckoview.BasicSelectionActionDelegate;
import org.mozilla.geckoview.ContentBlocking;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.WebExtension;
import org.mozilla.geckoview.WebRequestError;

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
import android.view.View;
import android.view.WindowManager;
import android.widget.ProgressBar;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Locale;

public class GeckoViewActivity extends AppCompatActivity {
    private static final String LOGTAG = "GeckoViewActivity";
    private static final String DEFAULT_URL = "about:blank";
    private static final String USE_MULTIPROCESS_EXTRA = "use_multiprocess";
    private static final String FULL_ACCESSIBILITY_TREE_EXTRA = "full_accessibility_tree";
    private static final String SEARCH_URI_BASE = "https://www.google.com/search?q=";
    private static final String ACTION_SHUTDOWN = "org.mozilla.geckoview_example.SHUTDOWN";
    private static final int REQUEST_FILE_PICKER = 1;
    private static final int REQUEST_PERMISSIONS = 2;
    private static final int REQUEST_WRITE_EXTERNAL_STORAGE = 3;

    private static GeckoRuntime sGeckoRuntime;
    private GeckoSession mGeckoSession;
    private GeckoView mGeckoView;
    private boolean mUseMultiprocess;
    private boolean mFullAccessibilityTree;
    private boolean mUseTrackingProtection;
    private boolean mUsePrivateBrowsing;
    private boolean mEnableRemoteDebugging;
    private boolean mKillProcessOnDestroy;

    private boolean mShowNotificationsRejected;
    private ArrayList<String> mAcceptedPersistentStorage = new ArrayList<String>();

    private LocationView mLocationView;
    private String mCurrentUri;
    private boolean mCanGoBack;
    private boolean mCanGoForward;
    private boolean mFullScreen;

    private ProgressBar mProgressView;

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
        mLocationView.setId(R.id.url_bar);
        getSupportActionBar().setCustomView(mLocationView,
                new ActionBar.LayoutParams(ActionBar.LayoutParams.MATCH_PARENT,
                        ActionBar.LayoutParams.WRAP_CONTENT));
        getSupportActionBar().setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM);

        mUseMultiprocess = getIntent().getBooleanExtra(USE_MULTIPROCESS_EXTRA, true);
        mEnableRemoteDebugging = true;
        mFullAccessibilityTree = getIntent().getBooleanExtra(FULL_ACCESSIBILITY_TREE_EXTRA, false);
        mProgressView = (ProgressBar) findViewById(R.id.page_progress);

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
                    .remoteDebuggingEnabled(mEnableRemoteDebugging)
                    .consoleOutput(true)
                    .contentBlocking(new ContentBlocking.Settings.Builder()
                        .categories(ContentBlocking.AT_DEFAULT)
                        .build())
                    .crashHandler(ExampleCrashHandler.class);

            sGeckoRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());
        }

        if(savedInstanceState == null) {
            mGeckoSession = (GeckoSession)getIntent().getParcelableExtra("session");
            if (mGeckoSession != null) {
                connectSession(mGeckoSession);

                if (!mGeckoSession.isOpen()) {
                    mGeckoSession.open(sGeckoRuntime);
                }

                mUseMultiprocess = mGeckoSession.getSettings().getUseMultiprocess();
                mFullAccessibilityTree = mGeckoSession.getSettings().getFullAccessibilityTree();

                mGeckoView.setSession(mGeckoSession);
            } else {
                mGeckoSession = createSession();
                mGeckoView.setSession(mGeckoSession, sGeckoRuntime);

                loadFromIntent(getIntent());
            }
        }

        mLocationView.setCommitListener(mCommitListener);
    }

    private GeckoSession createSession() {
        GeckoSession session = new GeckoSession(new GeckoSessionSettings.Builder()
                .useMultiprocess(mUseMultiprocess)
                .usePrivateMode(mUsePrivateBrowsing)
                .useTrackingProtection(mUseTrackingProtection)
                .fullAccessibilityTree(mFullAccessibilityTree)
                .build());

        connectSession(session);

        return session;
    }

    private void connectSession(GeckoSession session) {
        session.setContentDelegate(new ExampleContentDelegate());
        session.setHistoryDelegate(new ExampleHistoryDelegate());
        final ExampleContentBlockingDelegate cb = new ExampleContentBlockingDelegate();
        session.setContentBlockingDelegate(cb);
        session.setProgressDelegate(new ExampleProgressDelegate(cb));
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
        if(mGeckoSession != null) {
            mGeckoSession.close();
        }

        mGeckoSession = createSession();
        mGeckoSession.open(sGeckoRuntime);
        mGeckoView.setSession(mGeckoSession);
        mGeckoSession.loadUri(mCurrentUri != null ? mCurrentUri : DEFAULT_URL);
    }

    @Override
    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        if(savedInstanceState != null) {
            mGeckoSession = mGeckoView.getSession();
        } else {
            recreateSession();
        }
    }

    private void updateTrackingProtection(GeckoSession session) {
        session.getSettings().setUseTrackingProtection(mUseTrackingProtection);
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
        menu.findItem(R.id.action_remote_debugging).setChecked(mEnableRemoteDebugging);
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
            case R.id.action_remote_debugging:
                mEnableRemoteDebugging = !mEnableRemoteDebugging;
                sGeckoRuntime.getSettings().setRemoteDebuggingEnabled(mEnableRemoteDebugging);
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
        mGeckoSession
                .getUserAgent()
                .then(new GeckoResult.OnValueListener<String, Void>() {
            @Override
            public GeckoResult<Void> onValue(String userAgent) throws Throwable {
                downloadFile(response, userAgent);
                return null;
            }
        }, new GeckoResult.OnExceptionListener<Void>() {
            @Override
            public GeckoResult<Void> onException(Throwable exception) throws Throwable {
                // getUserAgent() cannot fail.
                throw new IllegalStateException("Could not get UserAgent string.");
            }
        });
    }

    private void downloadFile(GeckoSession.WebResponseInfo response, String userAgent) {
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
        req.addRequestHeader("User-Agent", userAgent);
        manager.enqueue(req);
    }

    private String mErrorTemplate;
    private String createErrorPage(final String error) {
        if (mErrorTemplate == null) {
            InputStream stream = null;
            BufferedReader reader = null;
            StringBuilder builder = new StringBuilder();
            try {
                stream = getResources().getAssets().open("error.html");
                reader = new BufferedReader(new InputStreamReader(stream));

                String line;
                while ((line = reader.readLine()) != null) {
                    builder.append(line);
                    builder.append("\n");
                }

                mErrorTemplate = builder.toString();
            } catch (IOException e) {
                Log.d(LOGTAG, "Failed to open error page template", e);
                return null;
            } finally {
                if (stream != null) {
                    try {
                        stream.close();
                    } catch (IOException e) {
                        Log.e(LOGTAG, "Failed to close error page template stream", e);
                    }
                }

                if (reader != null) {
                    try {
                        reader.close();
                    } catch (IOException e) {
                        Log.e(LOGTAG, "Failed to close error page template reader", e);
                    }
                }
            }
        }

        return mErrorTemplate.replace("$ERROR", error);
    }

    private class ExampleHistoryDelegate implements GeckoSession.HistoryDelegate {
        private final HashSet<String> mVisitedURLs;

        private ExampleHistoryDelegate() {
            mVisitedURLs = new HashSet<String>();
        }

        @Override
        public GeckoResult<Boolean> onVisited(GeckoSession session, String url,
                                              String lastVisitedURL, int flags) {
            Log.i(LOGTAG, "Visited URL: " + url);

            mVisitedURLs.add(url);
            return GeckoResult.fromValue(true);
        }

        @Override
        public GeckoResult<boolean[]> getVisited(GeckoSession session, String[] urls) {
            boolean[] visited = new boolean[urls.length];
            for (int i = 0; i < urls.length; i++) {
                visited[i] = mVisitedURLs.contains(urls[i]);
            }
            return GeckoResult.fromValue(visited);
        }

        @Override
        public void onHistoryStateChange(final GeckoSession session,
                                         final GeckoSession.HistoryDelegate.HistoryList state) {
            Log.i(LOGTAG, "History state updated");
        }
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
        public void onContextMenu(final GeckoSession session,
                                  int screenX, int screenY,
                                  final ContextElement element) {
            Log.d(LOGTAG, "onContextMenu screenX=" + screenX +
                          " screenY=" + screenY +
                          " type=" + element.type +
                          " linkUri=" + element.linkUri +
                          " title=" + element.title +
                          " alt=" + element.altText +
                          " srcUri=" + element.srcUri);
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

        @Override
        public void onFirstComposite(final GeckoSession session) {
            Log.d(LOGTAG, "onFirstComposite");
        }

        @Override
        public void onWebAppManifest(final GeckoSession session, JSONObject manifest) {
            Log.d(LOGTAG, "onWebAppManifest: " + manifest);
        }
    }

    private class ExampleProgressDelegate implements GeckoSession.ProgressDelegate {
        private ExampleContentBlockingDelegate mCb;

        private ExampleProgressDelegate(final ExampleContentBlockingDelegate cb) {
            mCb = cb;
        }

        @Override
        public void onPageStart(GeckoSession session, String url) {
            Log.i(LOGTAG, "Starting to load page at " + url);
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load start");
            mCb.clearCounters();
        }

        @Override
        public void onPageStop(GeckoSession session, boolean success) {
            Log.i(LOGTAG, "Stopping page load " + (success ? "successfully" : "unsuccessfully"));
            Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
                  " - page load stop");
            mCb.logCounters();
        }

        @Override
        public void onProgressChange(GeckoSession session, int progress) {
            Log.i(LOGTAG, "onProgressChange " + progress);

            mProgressView.setProgress(progress);

            if (progress > 0 && progress < 100) {
                mProgressView.setVisibility(View.VISIBLE);
            } else {
                mProgressView.setVisibility(View.GONE);
            }
        }

        @Override
        public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {
            Log.i(LOGTAG, "Security status changed to " + securityInfo.securityMode);
        }

        @Override
        public void onSessionStateChange(GeckoSession session, GeckoSession.SessionState state) {
            Log.i(LOGTAG, "New Session state: " + state.toString());
        }
    }

    private class ExamplePermissionDelegate implements GeckoSession.PermissionDelegate {

        public int androidPermissionRequestCode = 1;
        private Callback mCallback;

        class ExampleNotificationCallback implements GeckoSession.PermissionDelegate.Callback {
            private final GeckoSession.PermissionDelegate.Callback mCallback;
            ExampleNotificationCallback(final GeckoSession.PermissionDelegate.Callback callback) {
                mCallback = callback;
            }

            @Override
            public void reject() {
                mShowNotificationsRejected = true;
                mCallback.reject();
            }

            @Override
            public void grant() {
                mShowNotificationsRejected = false;
                mCallback.grant();
            }
        }

        class ExamplePersistentStorageCallback implements GeckoSession.PermissionDelegate.Callback {
            private final GeckoSession.PermissionDelegate.Callback mCallback;
            private final String mUri;
            ExamplePersistentStorageCallback(final GeckoSession.PermissionDelegate.Callback callback, String uri) {
                mCallback = callback;
                mUri = uri;
            }

            @Override
            public void reject() {
                mCallback.reject();
            }

            @Override
            public void grant() {
                mAcceptedPersistentStorage.add(mUri);
                mCallback.grant();
            }
        }

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
                                             final int type, final Callback callback) {
            final int resId;
            Callback contentPermissionCallback = callback;
            if (PERMISSION_GEOLOCATION == type) {
                resId = R.string.request_geolocation;
            } else if (PERMISSION_DESKTOP_NOTIFICATION == type) {
                if (mShowNotificationsRejected) {
                    Log.w(LOGTAG, "Desktop notifications already denied by user.");
                    callback.reject();
                    return;
                }
                resId = R.string.request_notification;
                contentPermissionCallback = new ExampleNotificationCallback(callback);
            } else if (PERMISSION_PERSISTENT_STORAGE == type) {
                if (mAcceptedPersistentStorage.contains(uri)) {
                    Log.w(LOGTAG, "Persistent Storage for "+ uri +" already granted by user.");
                    callback.grant();
                    return;
                }
                resId = R.string.request_storage;
                contentPermissionCallback = new ExamplePersistentStorageCallback(callback, uri);
            } else {
                Log.w(LOGTAG, "Unknown permission: " + type);
                callback.reject();
                return;
            }

            final String title = getString(resId, Uri.parse(uri).getAuthority());
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mGeckoSession.getPromptDelegate();
            prompt.onPermissionPrompt(session, title, contentPermissionCallback);
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
            // If we don't have device permissions at this point, just automatically reject the request
            // as we will have already have requested device permissions before getting to this point
            // and if we've reached here and we don't have permissions then that means that the user
            // denied them.
            if ((audio != null
                    && ContextCompat.checkSelfPermission(GeckoViewActivity.this,
                        Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
                || (video != null
                    && ContextCompat.checkSelfPermission(GeckoViewActivity.this,
                        Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED)) {
                callback.reject();
                return;
            }

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
        public GeckoResult<AllowOrDeny> onLoadRequest(final GeckoSession session,
                                                      final LoadRequest request) {
            Log.d(LOGTAG, "onLoadRequest=" + request.uri +
                  " triggerUri=" + request.triggerUri +
                  " where=" + request.target +
                  " isRedirect=" + request.isRedirect);

            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }

        @Override
        public GeckoResult<GeckoSession> onNewSession(final GeckoSession session, final String uri) {
            GeckoSession newSession = new GeckoSession(session.getSettings());

            Intent intent = new Intent(GeckoViewActivity.this, SessionActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
            intent.setAction(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(uri));
            intent.putExtra("session", newSession);

            startActivity(intent);

            return GeckoResult.fromValue(newSession);
        }

        private String categoryToString(final int category) {
            switch (category) {
                case WebRequestError.ERROR_CATEGORY_UNKNOWN:
                    return "ERROR_CATEGORY_UNKNOWN";
                case WebRequestError.ERROR_CATEGORY_SECURITY:
                    return "ERROR_CATEGORY_SECURITY";
                case WebRequestError.ERROR_CATEGORY_NETWORK:
                    return "ERROR_CATEGORY_NETWORK";
                case WebRequestError.ERROR_CATEGORY_CONTENT:
                    return "ERROR_CATEGORY_CONTENT";
                case WebRequestError.ERROR_CATEGORY_URI:
                    return "ERROR_CATEGORY_URI";
                case WebRequestError.ERROR_CATEGORY_PROXY:
                    return "ERROR_CATEGORY_PROXY";
                case WebRequestError.ERROR_CATEGORY_SAFEBROWSING:
                    return "ERROR_CATEGORY_SAFEBROWSING";
                default:
                    return "UNKNOWN";
            }
        }

        private String errorToString(final int error) {
            switch (error) {
                case WebRequestError.ERROR_UNKNOWN:
                    return "ERROR_UNKNOWN";
                case WebRequestError.ERROR_SECURITY_SSL:
                    return "ERROR_SECURITY_SSL";
                case WebRequestError.ERROR_SECURITY_BAD_CERT:
                    return "ERROR_SECURITY_BAD_CERT";
                case WebRequestError.ERROR_NET_RESET:
                    return "ERROR_NET_RESET";
                case WebRequestError.ERROR_NET_INTERRUPT:
                    return "ERROR_NET_INTERRUPT";
                case WebRequestError.ERROR_NET_TIMEOUT:
                    return "ERROR_NET_TIMEOUT";
                case WebRequestError.ERROR_CONNECTION_REFUSED:
                    return "ERROR_CONNECTION_REFUSED";
                case WebRequestError.ERROR_UNKNOWN_PROTOCOL:
                    return "ERROR_UNKNOWN_PROTOCOL";
                case WebRequestError.ERROR_UNKNOWN_HOST:
                    return "ERROR_UNKNOWN_HOST";
                case WebRequestError.ERROR_UNKNOWN_SOCKET_TYPE:
                    return "ERROR_UNKNOWN_SOCKET_TYPE";
                case WebRequestError.ERROR_UNKNOWN_PROXY_HOST:
                    return "ERROR_UNKNOWN_PROXY_HOST";
                case WebRequestError.ERROR_MALFORMED_URI:
                    return "ERROR_MALFORMED_URI";
                case WebRequestError.ERROR_REDIRECT_LOOP:
                    return "ERROR_REDIRECT_LOOP";
                case WebRequestError.ERROR_SAFEBROWSING_PHISHING_URI:
                    return "ERROR_SAFEBROWSING_PHISHING_URI";
                case WebRequestError.ERROR_SAFEBROWSING_MALWARE_URI:
                    return "ERROR_SAFEBROWSING_MALWARE_URI";
                case WebRequestError.ERROR_SAFEBROWSING_UNWANTED_URI:
                    return "ERROR_SAFEBROWSING_UNWANTED_URI";
                case WebRequestError.ERROR_SAFEBROWSING_HARMFUL_URI:
                    return "ERROR_SAFEBROWSING_HARMFUL_URI";
                case WebRequestError.ERROR_CONTENT_CRASHED:
                    return "ERROR_CONTENT_CRASHED";
                case WebRequestError.ERROR_OFFLINE:
                    return "ERROR_OFFLINE";
                case WebRequestError.ERROR_PORT_BLOCKED:
                    return "ERROR_PORT_BLOCKED";
                case WebRequestError.ERROR_PROXY_CONNECTION_REFUSED:
                    return "ERROR_PROXY_CONNECTION_REFUSED";
                case WebRequestError.ERROR_FILE_NOT_FOUND:
                    return "ERROR_FILE_NOT_FOUND";
                case WebRequestError.ERROR_FILE_ACCESS_DENIED:
                    return "ERROR_FILE_ACCESS_DENIED";
                case WebRequestError.ERROR_INVALID_CONTENT_ENCODING:
                    return "ERROR_INVALID_CONTENT_ENCODING";
                case WebRequestError.ERROR_UNSAFE_CONTENT_TYPE:
                    return "ERROR_UNSAFE_CONTENT_TYPE";
                case WebRequestError.ERROR_CORRUPTED_CONTENT:
                    return "ERROR_CORRUPTED_CONTENT";
                default:
                    return "UNKNOWN";
            }
        }

        private String createErrorPage(final int category, final int error) {
            if (mErrorTemplate == null) {
                InputStream stream = null;
                BufferedReader reader = null;
                StringBuilder builder = new StringBuilder();
                try {
                    stream = getResources().getAssets().open("error.html");
                    reader = new BufferedReader(new InputStreamReader(stream));

                    String line;
                    while ((line = reader.readLine()) != null) {
                        builder.append(line);
                        builder.append("\n");
                    }

                    mErrorTemplate = builder.toString();
                } catch (IOException e) {
                    Log.d(LOGTAG, "Failed to open error page template", e);
                    return null;
                } finally {
                    if (stream != null) {
                        try {
                            stream.close();
                        } catch (IOException e) {
                            Log.e(LOGTAG, "Failed to close error page template stream", e);
                        }
                    }

                    if (reader != null) {
                        try {
                            reader.close();
                        } catch (IOException e) {
                            Log.e(LOGTAG, "Failed to close error page template reader", e);
                        }
                    }
                }
            }

            return GeckoViewActivity.this.createErrorPage(categoryToString(category) + " : " + errorToString(error));
        }

        @Override
        public GeckoResult<String> onLoadError(final GeckoSession session, final String uri,
                                               final WebRequestError error) {
            Log.d(LOGTAG, "onLoadError=" + uri +
                  " error category=" + error.category +
                  " error=" + error.code);

            return GeckoResult.fromValue("data:text/html," + createErrorPage(error.category, error.code));
        }
    }

    private class ExampleContentBlockingDelegate
            implements ContentBlocking.Delegate {
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
        public void onContentBlocked(final GeckoSession session,
                                     final ContentBlocking.BlockEvent event) {
            Log.d(LOGTAG, "onContentBlocked " + event.categories +
                  " (" + event.uri + ")");
            if ((event.categories & ContentBlocking.AT_TEST) != 0) {
                mBlockedTest++;
            }
            if ((event.categories & ContentBlocking.AT_AD) != 0) {
                mBlockedAds++;
            }
            if ((event.categories & ContentBlocking.AT_ANALYTIC) != 0) {
                mBlockedAnalytics++;
            }
            if ((event.categories & ContentBlocking.AT_SOCIAL) != 0) {
                mBlockedSocial++;
            }
            if ((event.categories & ContentBlocking.AT_CONTENT) != 0) {
                mBlockedContent++;
            }
        }
    }
}
