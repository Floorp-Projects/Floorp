/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapps;

import java.io.File;
import java.io.IOException;
import java.util.List;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.customtabs.CustomTabsIntent;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.view.ActionMode;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.DoorHangerPopup;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoViewSettings;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.R;
import org.mozilla.gecko.customtabs.CustomTabsActivity;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.text.TextSelection;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.widget.ActionModePresenter;

public class WebAppActivity extends AppCompatActivity
                            implements ActionModePresenter,
                                       GeckoView.NavigationListener {
    private static final String LOGTAG = "WebAppActivity";

    public static final String MANIFEST_PATH = "MANIFEST_PATH";
    public static final String MANIFEST_URL = "MANIFEST_URL";
    private static final String SAVED_INTENT = "savedIntent";

    private GeckoView mGeckoView;
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
        }

        super.onCreate(savedInstanceState);

        mGeckoView = new GeckoView(this);
        mGeckoView.setNavigationListener(this);
        mGeckoView.setContentListener(new GeckoView.ContentListener() {
            public void onTitleChange(GeckoView view, String title) {}
            public void onContextMenu(GeckoView view, int screenX, int screenY,
                               String uri, String elementSrc) {}
            public void onFullScreen(GeckoView view, boolean fullScreen) {
                updateFullScreenContent(fullScreen);
            }
        });

        mPromptService = new PromptService(this, mGeckoView.getEventDispatcher());
        mDoorHangerPopup = new DoorHangerPopup(this, mGeckoView.getEventDispatcher());

        mTextSelection = TextSelection.Factory.create(mGeckoView, this);
        mTextSelection.create();

        final GeckoViewSettings settings = mGeckoView.getSettings();
        settings.setBoolean(GeckoViewSettings.USE_MULTIPROCESS, false);
        settings.setBoolean(
            GeckoViewSettings.USE_REMOTE_DEBUGGER,
            GeckoSharedPrefs.forApp(this).getBoolean(
                GeckoPreferences.PREFS_DEVTOOLS_REMOTE_USB_ENABLED, false));

        mManifest = WebAppManifest.fromFile(getIntent().getStringExtra(MANIFEST_URL),
                                            getIntent().getStringExtra(MANIFEST_PATH));

        updateFromManifest();

        mGeckoView.loadUri(mManifest.getStartUri().toString());

        setContentView(mGeckoView);
    }

    @Override
    public void onResume() {
        mGeckoView.setActive(true);
        super.onResume();
    }

    @Override
    public void onPause() {
        mGeckoView.setActive(false);
        super.onPause();
    }

    @Override
    public void onDestroy() {
        mTextSelection.destroy();
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
            mGeckoView.exitFullScreen();
        } else if (mCanGoBack) {
            mGeckoView.goBack();
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
            GeckoScreenOrientation.screenOrientationToAndroidOrientation(orientation);

        setRequestedOrientation(activityOrientation);
    }

    private void updateDisplayMode() {
        final String displayMode = mManifest.getDisplayMode();

        updateFullScreenMode(displayMode.equals("fullscreen"));

        GeckoViewSettings.DisplayMode mode;
        switch (displayMode) {
            case "standalone":
                mode = GeckoViewSettings.DisplayMode.STANDALONE;
                break;
            case "fullscreen":
                mode = GeckoViewSettings.DisplayMode.FULLSCREEN;
                break;
            case "minimal-ui":
                mode = GeckoViewSettings.DisplayMode.MINIMAL_UI;
                break;
            case "browser":
            default:
                mode = GeckoViewSettings.DisplayMode.BROWSER;
                break;
        }

        mGeckoView.getSettings().setInt(GeckoViewSettings.DISPLAY_MODE, mode.value());
    }

    /* GeckoView.NavigationListener */
    @Override
    public void onLocationChange(GeckoView view, String url) {
    }

    @Override
    public void onCanGoBack(GeckoView view, boolean canGoBack) {
        mCanGoBack = canGoBack;
    }

    @Override
    public void onCanGoForward(GeckoView view, boolean canGoForward) {
    }

    @Override
    public boolean onLoadUri(final GeckoView view, final String urlStr,
                             final TargetWindow where) {
        final Uri url = Uri.parse(urlStr);
        if (url == null) {
            // We can't really handle this, so deny it?
            Log.w(LOGTAG, "Failed to parse URL for navigation: " + urlStr);
            return true;
        }

        if (mManifest.isInScope(url) && where != TargetWindow.NEW) {
            // This is in scope and wants to load in the same frame, so
            // let Gecko handle it.
            return false;
        }

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
        tab.launchUrl(this, url);
        return true;
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
