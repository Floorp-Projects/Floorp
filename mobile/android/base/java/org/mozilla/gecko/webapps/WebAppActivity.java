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
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.customtabs.CustomTabsActivity;
import org.mozilla.gecko.DoorHangerPopup;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoViewSettings;
import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.permissions.Permissions;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.FileUtils;

public class WebAppActivity extends AppCompatActivity
                            implements GeckoView.NavigationListener {
    private static final String LOGTAG = "WebAppActivity";

    public static final String MANIFEST_PATH = "MANIFEST_PATH";
    private static final String SAVED_INTENT = "savedIntent";

    private GeckoView mGeckoView;
    private PromptService mPromptService;
    private DoorHangerPopup mDoorHangerPopup;

    private boolean mIsFullScreenMode;
    private boolean mIsFullScreenContent;
    private boolean mCanGoBack;
    private Uri mScope;

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

        final GeckoViewSettings settings = mGeckoView.getSettings();
        settings.setBoolean(GeckoViewSettings.USE_MULTIPROCESS, false);

        final Uri u = getIntent().getData();
        if (u != null) {
            mGeckoView.loadUri(u.toString());
        }

        setContentView(mGeckoView);

        loadManifest(getIntent().getStringExtra(MANIFEST_PATH));
    }

    @Override
    public void onDestroy() {
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

    private void loadManifest(String manifestPath) {
        if (TextUtils.isEmpty(manifestPath)) {
            Log.e(LOGTAG, "Missing manifest");
            return;
        }

        try {
            final File manifestFile = new File(manifestPath);
            final JSONObject manifest = FileUtils.readJSONObjectFromFile(manifestFile);
            final JSONObject manifestField = manifest.getJSONObject("manifest");

            if (AppConstants.Versions.feature21Plus) {
                loadManifestV21(manifest, manifestField);
            }

            updateScreenOrientation(manifestField);
            updateDisplayMode(manifestField);
        } catch (IOException | JSONException e) {
            Log.e(LOGTAG, "Failed to read manifest", e);
        }
    }

    // The customisations defined in the manifest only work on Android API 21+
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void loadManifestV21(JSONObject manifest, JSONObject manifestField) {
        final Integer color = readColorFromManifest(manifestField);
        final String name = readNameFromManifest(manifestField);
        final Bitmap icon = readIconFromManifest(manifest);
        mScope = readScopeFromManifest(manifest);
        final ActivityManager.TaskDescription taskDescription = (color == null)
            ? new ActivityManager.TaskDescription(name, icon)
            : new ActivityManager.TaskDescription(name, icon, color);

        updateStatusBarColorV21(color);
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

    private void updateScreenOrientation(JSONObject manifest) {
        String orientString = manifest.optString("orientation", null);
        if (orientString == null) {
            return;
        }

        GeckoScreenOrientation.ScreenOrientation orientation =
            GeckoScreenOrientation.screenOrientationFromString(orientString);
        int activityOrientation = GeckoScreenOrientation.screenOrientationToAndroidOrientation(orientation);

        setRequestedOrientation(activityOrientation);
    }

    private void updateDisplayMode(JSONObject manifest) {
        String displayMode = manifest.optString("display");

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

    private Integer readColorFromManifest(JSONObject manifest) {
        final String colorStr = manifest.optString("theme_color", null);
        if (colorStr != null) {
            return ColorUtil.parseStringColor(colorStr);
        }
        return null;
    }

    private String readNameFromManifest(JSONObject manifest) {
        String name = manifest.optString("name", null);
        if (name == null) {
            name = manifest.optString("short_name", null);
        }
        if (name == null) {
            name = manifest.optString("start_url", null);
        }
        return name;
    }

    private Bitmap readIconFromManifest(JSONObject manifest) {
        final String iconStr = manifest.optString("cached_icon", null);
        if (iconStr == null) {
            return null;
        }
        final LoadFaviconResult loadIconResult = FaviconDecoder
            .decodeDataURI(this, iconStr);
        if (loadIconResult == null) {
            return null;
        }
        return loadIconResult.getBestBitmap(GeckoAppShell.getPreferredIconSize());
    }

    private Uri readScopeFromManifest(JSONObject manifest) {
        final String scopeStr = manifest.optString("scope", null);
        if (scopeStr == null) {
            return null;
        }

        Uri res = Uri.parse(scopeStr);
        if (res.isRelative()) {
            // TODO: Handle this more correctly.
            return null;
        }

        return res;
    }

    private boolean isInScope(String url) {
        if (mScope == null) {
            return true;
        }

        final Uri uri = Uri.parse(url);

        if (!uri.getScheme().equals(mScope.getScheme())) {
            return false;
        }

        if (!uri.getHost().equals(mScope.getHost())) {
            return false;
        }

        final List<String> scopeSegments = mScope.getPathSegments();
        final List<String> urlSegments = uri.getPathSegments();

        if (scopeSegments.size() > urlSegments.size()) {
            return false;
        }

        for (int i = 0; i < scopeSegments.size(); i++) {
            if (!scopeSegments.get(i).equals(urlSegments.get(i))) {
                return false;
            }
        }

        return true;
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
    public boolean onLoadUri(final GeckoView view, final String uri,
                             final TargetWindow where) {
        if (isInScope(uri)) {
            view.loadUri(uri);
        } else {
            final Intent intent = new Intent(getIntent());
            intent.setClassName(getApplicationContext(),
                                CustomTabsActivity.class.getName());
            intent.setData(Uri.parse(uri));
            startActivity(intent);
        }
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
}
