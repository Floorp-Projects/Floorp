/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapps;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.customtabs.CustomTabsIntent;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.view.ActionMode;
import android.util.Log;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Toast;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.DoorHangerPopup;
import org.mozilla.gecko.FormAssistPopup;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.customtabs.CustomTabsActivity;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.text.TextSelection;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.geckoview.AllowOrDeny;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.GeckoView;

public class WebAppActivity extends AppCompatActivity
                            implements ActionModePresenter,
                                       GeckoSession.ContentDelegate,
                                       GeckoSession.NavigationDelegate {
    private static final String LOGTAG = "WebAppActivity";

    public static final String MANIFEST_PATH = "MANIFEST_PATH";
    public static final String MANIFEST_URL = "MANIFEST_URL";
    private static final String SAVED_INTENT = "savedIntent";

    private GeckoSession mGeckoSession;
    private GeckoView mGeckoView;
    private FormAssistPopup mFormAssistPopup;

    private PromptService mPromptService;
    private DoorHangerPopup mDoorHangerPopup;

    private ActionMode mActionMode;
    private TextSelection mTextSelection;

    private boolean mIsFullScreenMode;
    private boolean mIsFullScreenContent;
    private boolean mCanGoBack;

    private Uri mManifestUrl;
    private Uri mStartUrl;
    private Uri mScope;

    private WebAppManifest mManifest;

    private boolean mIsFirstLoad = true;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        if ((getIntent().getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0 &&
            savedInstanceState != null) {
            // Even though we're a single task activity, Android's task switcher has the
            // annoying habit of never updating its stored intent after our initial creation,
            // even if we've been subsequently started with a new intent.

            // This below is needed if we should ever decide to store a custom class as intent extra.
            savedInstanceState.setClassLoader(getClass().getClassLoader());

            Intent lastLaunchIntent = savedInstanceState.getParcelable(SAVED_INTENT);
            setIntent(lastLaunchIntent);
        } else {
            Telemetry.sendUIEvent(TelemetryContract.Event.PWA, TelemetryContract.Method.HOMESCREEN);
        }

        super.onCreate(savedInstanceState);
        setContentView(R.layout.webapp_activity);
        mGeckoView = (GeckoView) findViewById(R.id.pwa_gecko_view);

        final GeckoSessionSettings settings = new GeckoSessionSettings();
        settings.setBoolean(GeckoSessionSettings.USE_MULTIPROCESS, false);
        mGeckoSession = new GeckoSession(settings);
        mGeckoView.setSession(mGeckoSession, GeckoApplication.ensureRuntime(this));

        mGeckoSession.setNavigationDelegate(this);
        mGeckoSession.setContentDelegate(this);
        mGeckoSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
            @Override
            public void onPageStart(GeckoSession session, String url) {

            }

            @Override
            public void onPageStop(GeckoSession session, boolean success) {

            }

            @Override
            public void onProgressChange(GeckoSession session, int progress) {

            }

            @Override
            public void onSecurityChange(GeckoSession session, SecurityInformation security) {
                // We want to ignore the extraneous first about:blank load
                if (mIsFirstLoad && security.origin.startsWith("moz-nullprincipal:")) {
                    mIsFirstLoad = false;
                    return;
                }
                mIsFirstLoad = false;

                int message;
                if (!security.isSecure) {
                    if (SecurityInformation.CONTENT_LOADED == security.mixedModeActive) {
                        // Active Mixed Content loaded because user has disabled blocking.
                        message = R.string.mixed_content_protection_disabled;
                    } else if (SecurityInformation.CONTENT_LOADED == security.mixedModePassive) {
                        // Passive Mixed Content loaded.
                        if (SecurityInformation.CONTENT_BLOCKED == security.mixedModeActive) {
                            message = R.string.mixed_content_blocked_some;
                        } else {
                            message = R.string.mixed_content_display_loaded;
                        }
                    } else {
                        // Unencrypted connection with no mixed content.
                        message = R.string.identity_connection_insecure;
                    }
                    fallbackToFennec(getString(message));
                } else {
                    if (security.isException) {
                        message = R.string.identity_connection_insecure;
                        fallbackToFennec(getString(message));
                    }
                }
            }
        });

        mPromptService = new PromptService(this, mGeckoView.getEventDispatcher());
        mDoorHangerPopup = new DoorHangerPopup(this, mGeckoView.getEventDispatcher());

        mFormAssistPopup = (FormAssistPopup) findViewById(R.id.pwa_form_assist_popup);
        mFormAssistPopup.create(mGeckoView);

        mTextSelection = TextSelection.Factory.create(mGeckoView, this);
        mTextSelection.create();

        try {
            mManifest = WebAppManifest.fromFile(getIntent().getStringExtra(MANIFEST_URL),
                                                getIntent().getStringExtra(MANIFEST_PATH));
        } catch (Exception e) {
            Log.w(LOGTAG, "Cannot retrieve manifest, launching in Firefox:" + e);
            fallbackToFennec(null);
            return;
        }

        updateFromManifest();

        mGeckoSession.loadUri(mManifest.getStartUri().toString());
    }

    private void fallbackToFennec(String message) {
        if (message != null) {
            Toast.makeText(this, message, Toast.LENGTH_LONG).show();
        }

        try {
            Intent intent = new Intent(this, BrowserApp.class);
            intent.setAction(Intent.ACTION_VIEW);
            if (getIntent().getData() != null) {
                intent.setData(getIntent().getData());
                intent.setPackage(getPackageName());
                startActivity(intent);
            }
        } catch (Exception e2) {
            Log.e(LOGTAG, "Failed to fall back to launching in Firefox");
        }
        if (android.os.Build.VERSION.SDK_INT >= 21) {
            finishAndRemoveTask();
        } else {
            finish();
        }
    }

    @Override
    public void onDestroy() {
        mGeckoSession.close();
        mTextSelection.destroy();
        mFormAssistPopup.destroy();
        mDoorHangerPopup.destroy();
        mPromptService.destroy();
        super.onDestroy();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!ActivityHandlerHelper.handleActivityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void onRequestPermissionsResult(final int requestCode, final String[] permissions,
                                           final int[] grantResults) {
        Permissions.onRequestPermissionsResult(this, permissions, grantResults);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putParcelable(SAVED_INTENT, getIntent());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (hasFocus) {
            updateFullScreen();
        }
    }

    @Override
    public void onBackPressed() {
        if (mIsFullScreenContent) {
            mGeckoSession.exitFullScreen();
        } else if (mCanGoBack) {
            mGeckoSession.goBack();
        } else {
            super.onBackPressed();
        }
    }

    private void updateFromManifest() {
        if (AppConstants.Versions.feature21Plus) {
            updateTaskAndStatusBar();
        }

        updateScreenOrientation();
        updateDisplayMode();
    }

    // The customisations defined in the manifest only work on Android API 21+
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updateTaskAndStatusBar() {
        final Integer themeColor = mManifest.getThemeColor();
        final String name = mManifest.getName();
        final Bitmap icon = mManifest.getIcon();

        final ActivityManager.TaskDescription taskDescription = (themeColor == null)
            ? new ActivityManager.TaskDescription(name, icon)
            : new ActivityManager.TaskDescription(name, icon, themeColor);

        updateStatusBarColorV21(themeColor);
        setTaskDescription(taskDescription);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updateStatusBarColorV21(final Integer themeColor) {
        if (themeColor != null) {
            final Window window = getWindow();
            window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            window.setStatusBarColor(ColorUtil.darken(themeColor, 0.25));
        }
    }

    private void updateScreenOrientation() {
        final String orientString = mManifest.getOrientation();
        if (orientString == null) {
            return;
        }

        final GeckoScreenOrientation.ScreenOrientation orientation =
            GeckoScreenOrientation.screenOrientationFromString(orientString);
        final int activityOrientation =
            GeckoScreenOrientation.screenOrientationToActivityInfoOrientation(orientation);

        setRequestedOrientation(activityOrientation);
    }

    private void updateDisplayMode() {
        final String displayMode = mManifest.getDisplayMode();

        updateFullScreenMode(displayMode.equals("fullscreen"));

        int mode;
        switch (displayMode) {
            case "standalone":
                mode = GeckoSessionSettings.DISPLAY_MODE_STANDALONE;
                break;
            case "fullscreen":
                mode = GeckoSessionSettings.DISPLAY_MODE_FULLSCREEN;
                break;
            case "minimal-ui":
                mode = GeckoSessionSettings.DISPLAY_MODE_MINIMAL_UI;
                break;
            case "browser":
            default:
                mode = GeckoSessionSettings.DISPLAY_MODE_BROWSER;
                break;
        }

        mGeckoSession.getSettings().setInt(GeckoSessionSettings.DISPLAY_MODE, mode);
    }

    @Override // GeckoSession.NavigationDelegate
    public void onLocationChange(GeckoSession session, String url) {
    }

    @Override // GeckoSession.NavigationDelegate
    public void onCanGoBack(GeckoSession session, boolean canGoBack) {
        mCanGoBack = canGoBack;
    }

    @Override // GeckoSession.NavigationDelegate
    public void onCanGoForward(GeckoSession session, boolean canGoForward) {
    }

    @Override // GeckoSession.ContentDelegate
    public void onTitleChange(GeckoSession session, String title) {
    }

    @Override // GeckoSession.ContentDelegate
    public void onFocusRequest(GeckoSession session) {
        Intent intent = new Intent(getIntent());
        intent.addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        startActivity(intent);
    }

    @Override // GeckoSession.ContentDelegate
    public void onCloseRequest(GeckoSession session) {
        // Ignore
    }

    @Override // GeckoSession.ContentDelegate
    public void onContextMenu(GeckoSession session, int screenX, int screenY,
                              String uri, int elementType, String elementSrc) {
        final String content = uri != null ? uri : elementSrc != null ? elementSrc : "";
        final Uri validUri = WebApps.getValidURL(content);
        if (validUri == null) {
            return;
        }

        WebApps.openInFennec(validUri, WebAppActivity.this);
    }

    @Override // GeckoSession.ContentDelegate
    public void onExternalResponse(final GeckoSession session, final GeckoSession.WebResponseInfo request) {
        // Won't happen, as we don't use the GeckoView download support in Fennec
    }

    @Override // GeckoSession.ContentDelegate
    public void onCrash(final GeckoSession session) {
        // Won't happen, as we don't use e10s in Fennec
    }

    @Override // GeckoSession.ContentDelegate
    public void onFullScreen(GeckoSession session, boolean fullScreen) {
        updateFullScreenContent(fullScreen);
    }

    @Override
    public GeckoResult<AllowOrDeny> onLoadRequest(final GeckoSession session, final String urlStr,
                                                  final int target,
                                                  final int flags) {
        final Uri uri = Uri.parse(urlStr);
        if (uri == null) {
            // We can't really handle this, so deny it?
            Log.w(LOGTAG, "Failed to parse URL for navigation: " + urlStr);
            return GeckoResult.fromValue(AllowOrDeny.DENY);
        }

        if (mManifest.isInScope(uri) && target != TARGET_WINDOW_NEW) {
            // This is in scope and wants to load in the same frame, so
            // let Gecko handle it.
            return GeckoResult.fromValue(AllowOrDeny.ALLOW);
        }

        if ("http".equals(uri.getScheme()) || "https".equals(uri.getScheme()) ||
            "data".equals(uri.getScheme()) || "blob".equals(uri.getScheme())) {
            final CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder()
                .addDefaultShareMenuItem()
                .setStartAnimations(this, R.anim.slide_in_right, R.anim.slide_out_left)
                .setExitAnimations(this, R.anim.slide_in_left, R.anim.slide_out_right);

            final Integer themeColor = mManifest.getThemeColor();
            if (themeColor != null) {
                builder.setToolbarColor(themeColor);
            }

            final CustomTabsIntent tab = builder.build();
            tab.intent.setClass(this, CustomTabsActivity.class);
            tab.launchUrl(this, uri);
        } else {
            final Intent intent = new Intent();
            intent.setAction(Intent.ACTION_VIEW);
            intent.setData(uri);
            try {
                startActivity(intent);
            } catch (ActivityNotFoundException e) {
                Log.w(LOGTAG, "No activity handler found for: " + urlStr);
            }
        }

        return GeckoResult.fromValue(AllowOrDeny.DENY);
    }

    @Override
    public GeckoResult<GeckoSession> onNewSession(final GeckoSession session, final String uri) {
        // We should never get here because we abort loads that need a new session in onLoadRequest()
        throw new IllegalStateException("Unexpected new session");
    }

    @Override
    public GeckoResult<String> onLoadError(final GeckoSession session, final String urlStr,
                                           final int category, final int error) {
        return null;
    }

    private void updateFullScreen() {
        boolean fullScreen = mIsFullScreenContent || mIsFullScreenMode;
        if (ActivityUtils.isFullScreen(this) == fullScreen) {
            return;
        }

        ActivityUtils.setFullScreen(this, fullScreen);
    }

    private void updateFullScreenContent(boolean fullScreen) {
        mIsFullScreenContent = fullScreen;
        updateFullScreen();
    }

    private void updateFullScreenMode(boolean fullScreen) {
        mIsFullScreenMode = fullScreen;
        updateFullScreen();
    }

    @Override // ActionModePresenter
    public void startActionMode(final ActionMode.Callback callback) {
        endActionMode();
        mActionMode = startSupportActionMode(callback);
    }

    @Override // ActionModePresenter
    public void endActionMode() {
        if (mActionMode != null) {
            mActionMode.finish();
            mActionMode = null;
        }
    }
}
