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
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.view.ActionMode;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoView;
import org.mozilla.gecko.GeckoViewSettings;
import org.mozilla.gecko.SingleTabActivity;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.gecko.widget.AnchoredPopup;

import static org.mozilla.gecko.Tabs.TabEvents;

public class WebAppActivity extends AppCompatActivity
                            implements GeckoView.NavigationListener {
    private static final String LOGTAG = "WebAppActivity";

    public static final String MANIFEST_PATH = "MANIFEST_PATH";
    private static final String SAVED_INTENT = "savedIntent";

    private TextView mUrlView;
    private GeckoView mGeckoView;
    private PromptService mPromptService;

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

        setContentView(R.layout.customtabs_activity);

        final Toolbar toolbar = (Toolbar) findViewById(R.id.actionbar);
        setSupportActionBar(toolbar);

        final ActionBar actionBar = getSupportActionBar();
        actionBar.setCustomView(R.layout.webapps_action_bar_custom_view);
        actionBar.setDisplayShowCustomEnabled(true);
        actionBar.setDisplayShowTitleEnabled(false);
        actionBar.hide();

        final View customView = actionBar.getCustomView();
        mUrlView = (TextView) customView.findViewById(R.id.webapps_action_bar_url);

        mGeckoView = (GeckoView) findViewById(R.id.gecko_view);

        mGeckoView.setNavigationListener(this);

        mPromptService = new PromptService(this, mGeckoView.getEventDispatcher());

        final GeckoViewSettings settings = mGeckoView.getSettings();
        settings.setBoolean(GeckoViewSettings.USE_MULTIPROCESS, false);

        final Uri u = getIntent().getData();
        if (u != null) {
            mGeckoView.loadUri(u.toString());
        }

        loadManifest(getIntent().getStringExtra(MANIFEST_PATH));
    }

    @Override
    public void onDestroy() {
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
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putParcelable(SAVED_INTENT, getIntent());
    }

    private void loadManifest(String manifestPath) {
        if (AppConstants.Versions.feature21Plus) {
            loadManifestV21(manifestPath);
        }
    }

    // The customisations defined in the manifest only work on Android API 21+
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void loadManifestV21(String manifestPath) {
        if (TextUtils.isEmpty(manifestPath)) {
            Log.e(LOGTAG, "Missing manifest");
            return;
        }

        try {
            final File manifestFile = new File(manifestPath);
            final JSONObject manifest = FileUtils.readJSONObjectFromFile(manifestFile);
            final JSONObject manifestField = manifest.getJSONObject("manifest");
            final Integer color = readColorFromManifest(manifestField);
            final String name = readNameFromManifest(manifestField);
            final Bitmap icon = readIconFromManifest(manifest);
            mScope = readScopeFromManifest(manifest, manifestPath);
            final ActivityManager.TaskDescription taskDescription = (color == null)
                    ? new ActivityManager.TaskDescription(name, icon)
                    : new ActivityManager.TaskDescription(name, icon, color);

            updateStatusBarColorV21(color);
            setTaskDescription(taskDescription);

        } catch (IOException | JSONException e) {
            Log.e(LOGTAG, "Failed to read manifest", e);
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updateStatusBarColorV21(final Integer themeColor) {
        if (themeColor != null) {
            final Window window = getWindow();
            window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            window.setStatusBarColor(ColorUtil.darken(themeColor, 0.25));
        }
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

    private Uri readScopeFromManifest(JSONObject manifest, String manifestPath) {
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
        if (isInScope(url)) {
            getSupportActionBar().hide();
        } else {
            getSupportActionBar().show();
        }

        mUrlView.setText(url);
    }

    @Override
    public void onCanGoBack(GeckoView view, boolean canGoBack) {
    }

    @Override
    public void onCanGoForward(GeckoView view, boolean canGoForward) {
    }
}
