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
import org.mozilla.geckoview.GeckoWebExecutor;
import org.mozilla.geckoview.WebExtension;
import org.mozilla.geckoview.WebExtensionController;
import org.mozilla.geckoview.WebNotification;
import org.mozilla.geckoview.WebNotificationDelegate;
import org.mozilla.geckoview.WebRequest;
import org.mozilla.geckoview.WebRequestError;
import org.mozilla.geckoview.RuntimeTelemetry;
import org.mozilla.geckoview.WebResponse;

import android.Manifest;
import android.app.Activity;
import android.app.DownloadManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.SystemClock;
import android.security.keystore.KeyProperties;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.ActivityCompat;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
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
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Locale;

public class GeckoViewActivity extends AppCompatActivity {
    private static final String LOGTAG = "GeckoViewActivity";
    private static final String USE_MULTIPROCESS_EXTRA = "use_multiprocess";
    private static final String FULL_ACCESSIBILITY_TREE_EXTRA = "full_accessibility_tree";
    private static final String SEARCH_URI_BASE = "https://www.google.com/search?q=";
    private static final String ACTION_SHUTDOWN = "org.mozilla.geckoview_example.SHUTDOWN";
    private static final String CHANNEL_ID = "GeckoViewExample";
    private static final int REQUEST_FILE_PICKER = 1;
    private static final int REQUEST_PERMISSIONS = 2;
    private static final int REQUEST_WRITE_EXTERNAL_STORAGE = 3;

    private static GeckoRuntime sGeckoRuntime;
    private TabSessionManager mTabSessionManager;
    private GeckoView mGeckoView;
    private boolean mUseMultiprocess;
    private boolean mFullAccessibilityTree;
    private boolean mUseTrackingProtection;
    private boolean mUsePrivateBrowsing;
    private boolean mEnableRemoteDebugging;
    private boolean mKillProcessOnDestroy;
    private boolean mDesktopMode;

    private boolean mShowNotificationsRejected;
    private ArrayList<String> mAcceptedPersistentStorage = new ArrayList<String>();

    private ToolbarLayout mToolbarView;
    private String mCurrentUri;
    private boolean mCanGoBack;
    private boolean mCanGoForward;
    private boolean mFullScreen;

    private HashMap<String, Integer> mNotificationIDMap = new HashMap<>();
    private HashMap<Integer, WebNotification> mNotificationMap = new HashMap<>();
    private int mLastID = 100;

    private ProgressBar mProgressView;

    private LinkedList<GeckoSession.WebResponseInfo> mPendingDownloads = new LinkedList<>();

    private LocationView.CommitListener mCommitListener = new LocationView.CommitListener() {
        @Override
        public void onCommit(String text) {
            if ((text.contains(".") || text.contains(":")) && !text.contains(" ")) {
                mTabSessionManager.getCurrentSession().loadUri(text);
            } else {
                mTabSessionManager.getCurrentSession().loadUri(SEARCH_URI_BASE + text);
            }
            mGeckoView.requestFocus();
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(LOGTAG, "zerdatime " + SystemClock.elapsedRealtime() +
              " - application start");
        createNotificationChannel();
        setContentView(R.layout.geckoview_activity);
        mGeckoView = findViewById(R.id.gecko_view);

        mTabSessionManager = new TabSessionManager();

        setSupportActionBar(findViewById(R.id.toolbar));

        mToolbarView = new ToolbarLayout(this, mTabSessionManager);
        mToolbarView.setId(R.id.toolbar_layout);
        mToolbarView.setTabListener(this::switchToSessionAtIndex);

        getSupportActionBar().setCustomView(mToolbarView,
                new ActionBar.LayoutParams(ActionBar.LayoutParams.MATCH_PARENT,
                        ActionBar.LayoutParams.WRAP_CONTENT));
        getSupportActionBar().setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM);

        mUseMultiprocess = getIntent().getBooleanExtra(USE_MULTIPROCESS_EXTRA, true);
        mEnableRemoteDebugging = true;
        mFullAccessibilityTree = getIntent().getBooleanExtra(FULL_ACCESSIBILITY_TREE_EXTRA, false);
        mProgressView = findViewById(R.id.page_progress);

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
                        .antiTracking(ContentBlocking.AntiTracking.DEFAULT |
                                      ContentBlocking.AntiTracking.STP)
                        .safeBrowsing(ContentBlocking.SafeBrowsing.DEFAULT)
                        .cookieBehavior(ContentBlocking.CookieBehavior.ACCEPT_NON_TRACKERS)
                        .build())
                    .crashHandler(ExampleCrashHandler.class)
                    .telemetryDelegate(new ExampleTelemetryDelegate());

            sGeckoRuntime = GeckoRuntime.create(this, runtimeSettingsBuilder.build());

            sGeckoRuntime.getWebExtensionController().setTabDelegate(new WebExtensionController.TabDelegate() {
                @Override
                public GeckoResult<GeckoSession> onNewTab(WebExtension source, String uri) {
                    final TabSession newSession = createSession();
                    mToolbarView.updateTabCount();
                    return GeckoResult.fromValue(newSession);
                }
                @Override
                public GeckoResult<AllowOrDeny> onCloseTab(WebExtension source, GeckoSession session) {
                    TabSession tabSession = mTabSessionManager.getSession(session);
                    closeTab(tabSession);
                    return GeckoResult.fromValue(AllowOrDeny.ALLOW);
                }
            });

            // `getSystemService` call requires API level 23
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
                sGeckoRuntime.setWebNotificationDelegate(new WebNotificationDelegate() {
                    NotificationManager notificationManager = getSystemService(NotificationManager.class);
                    @Override
                    public void onShowNotification(@NonNull WebNotification notification) {
                        Intent clickIntent = new Intent(GeckoViewActivity.this, GeckoViewActivity.class);
                        clickIntent.putExtra("onClick",notification.tag);
                        PendingIntent dismissIntent = PendingIntent.getActivity(GeckoViewActivity.this, mLastID, clickIntent, 0);

                        Notification.Builder builder = new Notification.Builder(GeckoViewActivity.this)
                                .setContentTitle(notification.title)
                                .setContentText(notification.text)
                                .setSmallIcon(R.drawable.ic_status_logo)
                                .setContentIntent(dismissIntent)
                                .setAutoCancel(true);

                        mNotificationIDMap.put(notification.tag, mLastID);
                        mNotificationMap.put(mLastID, notification);

                        if (notification.imageUrl != null && notification.imageUrl.length() > 0) {
                            final GeckoWebExecutor executor = new GeckoWebExecutor(sGeckoRuntime);

                            GeckoResult<WebResponse> response = executor.fetch(
                                    new WebRequest.Builder(notification.imageUrl)
                                            .addHeader("Accept", "image")
                                            .build());
                            response.accept(value -> {
                                Bitmap bitmap = BitmapFactory.decodeStream(value.body);
                                builder.setLargeIcon(Icon.createWithBitmap(bitmap));
                                notificationManager.notify(mLastID++, builder.build());
                            });
                        } else {
                            notificationManager.notify(mLastID++, builder.build());
                        }

                    }

                    @Override
                    public void onCloseNotification(@NonNull WebNotification notification) {
                        if (mNotificationIDMap.containsKey(notification.tag)) {
                            int id = mNotificationIDMap.get(notification.tag);
                            notificationManager.cancel(id);
                            mNotificationMap.remove(id);
                            mNotificationIDMap.remove(notification.tag);
                        }
                    }
                });


            }
        }

        if(savedInstanceState == null) {
            TabSession session = getIntent().getParcelableExtra("session");
            if (session != null) {
                connectSession(session);

                if (!session.isOpen()) {
                    session.open(sGeckoRuntime);
                }

                mUseMultiprocess = session.getSettings().getUseMultiprocess();
                mFullAccessibilityTree = session.getSettings().getFullAccessibilityTree();

                mTabSessionManager.addSession(session);
                setGeckoViewSession(session);
            } else {
                session = createSession();
                session.open(sGeckoRuntime);
                mTabSessionManager.setCurrentSession(session);
                mGeckoView.setSession(session);
            }
            loadFromIntent(getIntent());
        }

        mToolbarView.getLocationView().setCommitListener(mCommitListener);
        mToolbarView.updateTabCount();
    }

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = getString(R.string.app_name);
            String description = getString(R.string.activity_label);
            int importance = NotificationManager.IMPORTANCE_DEFAULT;
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
            channel.setDescription(description);
            // Register the channel with the system; you can't change the importance
            // or other notification behaviors after this
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }
    
    private TabSession createSession() {
        TabSession session = mTabSessionManager.newSession(new GeckoSessionSettings.Builder()
                .useMultiprocess(mUseMultiprocess)
                .usePrivateMode(mUsePrivateBrowsing)
                .useTrackingProtection(mUseTrackingProtection)
                .fullAccessibilityTree(mFullAccessibilityTree)
                .viewportMode(mDesktopMode
                        ? GeckoSessionSettings.VIEWPORT_MODE_DESKTOP
                        : GeckoSessionSettings.VIEWPORT_MODE_MOBILE)
                .userAgentMode(mDesktopMode
                        ? GeckoSessionSettings.USER_AGENT_MODE_DESKTOP
                        : GeckoSessionSettings.USER_AGENT_MODE_MOBILE)
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

        session.setMediaDelegate(new ExampleMediaDelegate(this));

        session.setSelectionActionDelegate(new BasicSelectionActionDelegate(this));

        updateTrackingProtection(session);
        updateDesktopMode(session);
    }

    private void recreateSession() {
        recreateSession(mTabSessionManager.getCurrentSession());
    }

    private void recreateSession(TabSession session) {
        if(session != null) {
            mTabSessionManager.closeSession(session);
        }

        session = createSession();
        session.open(sGeckoRuntime);
        mTabSessionManager.setCurrentSession(session);
        mGeckoView.setSession(session);
        if (mCurrentUri != null) {
            session.loadUri(mCurrentUri);
        }
    }

    @Override
    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        if(savedInstanceState != null) {
            mTabSessionManager.setCurrentSession((TabSession)mGeckoView.getSession());
        } else {
            recreateSession();
        }
    }

    private void updateDesktopMode(GeckoSession session) {
        session.getSettings().setViewportMode(mDesktopMode
                ? GeckoSessionSettings.VIEWPORT_MODE_DESKTOP
                : GeckoSessionSettings.VIEWPORT_MODE_MOBILE);
        session.getSettings().setUserAgentMode(mDesktopMode
                ? GeckoSessionSettings.USER_AGENT_MODE_DESKTOP
                : GeckoSessionSettings.USER_AGENT_MODE_MOBILE);
    }

    private void updateTrackingProtection(GeckoSession session) {
        session.getSettings().setUseTrackingProtection(mUseTrackingProtection);
        sGeckoRuntime.getSettings().getContentBlocking()
                .setStrictSocialTrackingProtection(mUseTrackingProtection);
    }

    @Override
    public void onBackPressed() {
        GeckoSession session = mTabSessionManager.getCurrentSession();
        if (mFullScreen && session != null) {
            session.exitFullScreen();
            return;
        }

        if (mCanGoBack && session != null) {
            session.goBack();
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
        menu.findItem(R.id.desktop_mode).setChecked(mDesktopMode);
        menu.findItem(R.id.action_remote_debugging).setChecked(mEnableRemoteDebugging);
        menu.findItem(R.id.action_forward).setEnabled(mCanGoForward);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        GeckoSession session = mTabSessionManager.getCurrentSession();
        switch (item.getItemId()) {
            case R.id.action_reload:
                session.reload();
                break;
            case R.id.action_forward:
                session.goForward();
                break;
            case R.id.action_e10s:
                mUseMultiprocess = !mUseMultiprocess;
                recreateSession();
                break;
            case R.id.action_tp:
                mUseTrackingProtection = !mUseTrackingProtection;
                updateTrackingProtection(session);
                session.reload();
                break;
            case R.id.desktop_mode:
                mDesktopMode = !mDesktopMode;
                updateDesktopMode(session);
                session.reload();
                break;
            case R.id.action_pb:
                mUsePrivateBrowsing = !mUsePrivateBrowsing;
                recreateSession();
                break;
            case R.id.action_new_tab:
                createNewTab();
                break;
            case R.id.action_close_tab:
                closeTab((TabSession)session);
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

    private void createNewTab() {
        TabSession newSession = createSession();
        newSession.open(sGeckoRuntime);
        setGeckoViewSession(newSession);
        mToolbarView.updateTabCount();
    }

    private void closeTab(TabSession session) {
        if(mTabSessionManager.sessionCount() > 1) {
            mTabSessionManager.closeSession(session);
            TabSession tabSession = mTabSessionManager.getCurrentSession();
            setGeckoViewSession(tabSession);
            tabSession.reload();
            mToolbarView.updateTabCount();
        } else {
            recreateSession(session);
        }
    }

    private void switchToSessionAtIndex(int index) {
        TabSession nextSession = mTabSessionManager.getSession(index);
        TabSession currentSession = mTabSessionManager.getCurrentSession();
        if(nextSession != currentSession) {
            setGeckoViewSession(nextSession);
            mCurrentUri = nextSession.getUri();
            mToolbarView.getLocationView().setText(mCurrentUri);
        }
    }

    private void setGeckoViewSession(TabSession session) {
        mGeckoView.releaseSession();
        if(!session.isOpen()) {
            session.open(sGeckoRuntime);
        }
        mGeckoView.setSession(session);
        mTabSessionManager.setCurrentSession(session);
    }

    @Override
    public void onDestroy() {
        if (mKillProcessOnDestroy) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }

        super.onDestroy();
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

        if (intent.hasExtra("onClick")) {
            int key = intent.getExtras().getInt("onClick");
            WebNotification notification = mNotificationMap.get(key);
            if (notification != null) {
                notification.click();
                mNotificationMap.remove(key);
            }
        }

        setIntent(intent);

        if (intent.getData() != null) {
            loadFromIntent(intent);
        }
    }


    private void loadFromIntent(final Intent intent) {
        final Uri uri = intent.getData();
        if (uri != null) {
            mTabSessionManager.getCurrentSession().loadUri(uri.toString());
        }
    }

    @Override
    protected void onActivityResult(final int requestCode, final int resultCode,
                                    final Intent data) {
        if (requestCode == REQUEST_FILE_PICKER) {
            final BasicGeckoViewPrompt prompt = (BasicGeckoViewPrompt)
                    mTabSessionManager.getCurrentSession().getPromptDelegate();
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
                    mTabSessionManager.getCurrentSession().getPermissionDelegate();
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
        mTabSessionManager.getCurrentSession()
                .getUserAgent()
                .accept(userAgent -> downloadFile(response, userAgent),
                        exception -> {
                    throw new IllegalStateException("Could not get UserAgent string.");
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
            TabSession tabSession = mTabSessionManager.getSession(session);
            if (tabSession != null ) {
                tabSession.setTitle(title);
            }
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
            if (session == mTabSessionManager.getCurrentSession()) {
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
                    mTabSessionManager.getCurrentSession().getPromptDelegate();
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
                    mTabSessionManager.getCurrentSession().getPromptDelegate();
            prompt.onMediaPrompt(session, title, video, audio, videoNames, audioNames, callback);
        }
    }

    private class ExampleNavigationDelegate implements GeckoSession.NavigationDelegate {
        @Override
        public void onLocationChange(GeckoSession session, final String url) {
            mToolbarView.getLocationView().setText(url);
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
            final TabSession newSession = createSession();
            mToolbarView.updateTabCount();
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
        private int mBlockedStp = 0;

        private void clearCounters() {
            mBlockedAds = 0;
            mBlockedAnalytics = 0;
            mBlockedSocial = 0;
            mBlockedContent = 0;
            mBlockedTest = 0;
            mBlockedStp = 0;
        }

        private void logCounters() {
            Log.d(LOGTAG, "Trackers blocked: " + mBlockedAds + " ads, " +
                  mBlockedAnalytics + " analytics, " +
                  mBlockedSocial + " social, " +
                  mBlockedContent + " content, " +
                  mBlockedTest + " test, " +
                  mBlockedStp + "stp");
        }

        @Override
        public void onContentBlocked(final GeckoSession session,
                                     final ContentBlocking.BlockEvent event) {
            Log.d(LOGTAG, "onContentBlocked" +
                  " AT: " + event.getAntiTrackingCategory() +
                  " SB: " + event.getSafeBrowsingCategory() +
                  " CB: " + event.getCookieBehaviorCategory() +
                  " URI: " + event.uri);
            if ((event.getAntiTrackingCategory() &
                  ContentBlocking.AntiTracking.TEST) != 0) {
                mBlockedTest++;
            }
            if ((event.getAntiTrackingCategory() &
                  ContentBlocking.AntiTracking.AD) != 0) {
                mBlockedAds++;
            }
            if ((event.getAntiTrackingCategory() &
                  ContentBlocking.AntiTracking.ANALYTIC) != 0) {
                mBlockedAnalytics++;
            }
            if ((event.getAntiTrackingCategory() &
                  ContentBlocking.AntiTracking.SOCIAL) != 0) {
                mBlockedSocial++;
            }
            if ((event.getAntiTrackingCategory() &
                  ContentBlocking.AntiTracking.CONTENT) != 0) {
                mBlockedContent++;
            }
            if ((event.getAntiTrackingCategory() &
                  ContentBlocking.AntiTracking.STP) != 0) {
                mBlockedStp++;
            }
        }

        @Override
        public void onContentLoaded(final GeckoSession session,
                                    final ContentBlocking.BlockEvent event) {
            Log.d(LOGTAG, "onContentLoaded" +
                  " AT: " + event.getAntiTrackingCategory() +
                  " SB: " + event.getSafeBrowsingCategory() +
                  " CB: " + event.getCookieBehaviorCategory() +
                  " URI: " + event.uri);
        }
    }

    private class ExampleMediaDelegate
            implements GeckoSession.MediaDelegate {
        private Integer mLastNotificationId = 100;
        private Integer mNotificationId;
        final private Activity mActivity;

        public ExampleMediaDelegate(Activity activity) {
            mActivity = activity;
        }

        @Override
        public void onRecordingStatusChanged(@NonNull GeckoSession session, RecordingDevice[] devices) {
            String message;
            int icon;
            NotificationManagerCompat notificationManager = NotificationManagerCompat.from(mActivity);
            RecordingDevice camera = null;
            RecordingDevice microphone = null;

            for (RecordingDevice device : devices) {
                if (device.type == RecordingDevice.Type.CAMERA) {
                    camera = device;
                } else if (device.type == RecordingDevice.Type.MICROPHONE) {
                    microphone = device;
                }
            }
            if (camera != null && microphone != null) {
                Log.d(LOGTAG, "ExampleDeviceDelegate:onRecordingDeviceEvent display alert_mic_camera");
                message = getResources().getString(R.string.device_sharing_camera_and_mic);
                icon = R.drawable.alert_mic_camera;
            } else if (camera != null) {
                Log.d(LOGTAG, "ExampleDeviceDelegate:onRecordingDeviceEvent display alert_camera");
                message = getResources().getString(R.string.device_sharing_camera);
                icon = R.drawable.alert_camera;
            } else if (microphone != null){
                Log.d(LOGTAG, "ExampleDeviceDelegate:onRecordingDeviceEvent display alert_mic");
                message = getResources().getString(R.string.device_sharing_microphone);
                icon = R.drawable.alert_mic;
            } else {
                Log.d(LOGTAG, "ExampleDeviceDelegate:onRecordingDeviceEvent dismiss any notifications");
                if (mNotificationId != null) {
                    notificationManager.cancel(mNotificationId);
                    mNotificationId = null;
                }
                return;
            }
            if (mNotificationId == null) {
                mNotificationId = ++mLastNotificationId;
            }

            Intent intent = new Intent(mActivity, GeckoViewActivity.class);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            PendingIntent pendingIntent = PendingIntent.getActivity(mActivity.getApplicationContext(), 0, intent, 0);

            NotificationCompat.Builder builder = new NotificationCompat.Builder(mActivity.getApplicationContext(), CHANNEL_ID)
                    .setSmallIcon(icon)
                    .setContentTitle(getResources().getString(R.string.app_name))
                    .setContentText(message)
                    .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                    .setContentIntent(pendingIntent)
                    .setCategory(NotificationCompat.CATEGORY_SERVICE);

            notificationManager.notify(mNotificationId, builder.build());
        }
    }

    private final class ExampleTelemetryDelegate
            implements RuntimeTelemetry.Delegate {
        @Override
        public void onTelemetryReceived(final @NonNull RuntimeTelemetry.Metric metric) {
            Log.d(LOGTAG, "onTelemetryReceived " + metric);
        }
    }
}
