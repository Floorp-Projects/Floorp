/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapps;

import java.io.File;
import java.io.IOException;

import android.app.ActivityManager;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.support.v7.widget.Toolbar;
import android.support.v7.app.ActionBar;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.json.JSONObject;
import org.json.JSONException;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.SingleTabActivity;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.icons.decoders.FaviconDecoder;
import org.mozilla.gecko.icons.decoders.LoadFaviconResult;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.widget.AnchoredPopup;

import static org.mozilla.gecko.Tabs.TabEvents;

public class WebAppActivity extends SingleTabActivity {
    private static final String LOGTAG = "WebAppActivity";

    public static final String MANIFEST_PATH = "MANIFEST_PATH";
    private static final String SAVED_INTENT = "savedIntent";

    private TextView mUrlView;
    private View doorhangerOverlay;


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

        final Toolbar toolbar = (Toolbar) findViewById(R.id.actionbar);
        setSupportActionBar(toolbar);

        final ProgressBar progressBar = (ProgressBar) findViewById(R.id.page_progress);
        progressBar.setVisibility(View.GONE);

        final ActionBar actionBar = getSupportActionBar();
        actionBar.setCustomView(R.layout.webapps_action_bar_custom_view);
        actionBar.setDisplayShowCustomEnabled(true);
        actionBar.setDisplayShowTitleEnabled(false);
        actionBar.hide();

        doorhangerOverlay = findViewById(R.id.custom_tabs_doorhanger_overlay);

        final View customView = actionBar.getCustomView();
        mUrlView = (TextView) customView.findViewById(R.id.webapps_action_bar_url);

        EventDispatcher.getInstance().registerUiThreadListener(this,
                "Website:AppEntered",
                "Website:AppLeft",
                null);

        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    public View getDoorhangerOverlay() {
        return doorhangerOverlay;
    }

    @Override
    public int getLayout() {
        return R.layout.customtabs_activity;
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        if (message == null ||
                !message.containsKey("tabId") || message.getInt("tabId") != mLastSelectedTabId) {
            return;
        }

        switch (event) {
            case "Website:AppEntered":
                getSupportActionBar().hide();
                break;

            case "Website:AppLeft":
                getSupportActionBar().show();
                break;
        }
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        if (tab == null || !Tabs.getInstance().isSelectedTab(tab) ||
                tab.getType() != Tab.TabType.WEBAPP) {
            return;
        }

        if (msg == TabEvents.LOCATION_CHANGE ||
                msg == TabEvents.SELECTED) {
            mUrlView.setText(tab.getURL());
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putParcelable(SAVED_INTENT, getIntent());
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        EventDispatcher.getInstance().unregisterUiThreadListener(this,
                "Website:AppEntered",
                "Website:AppLeft",
                null);
        Tabs.unregisterOnTabsChangedListener(this);
    }

    @Override
    protected int getNewTabFlags() {
        return Tabs.LOADURL_WEBAPP | super.getNewTabFlags();
    }

    @Override
    protected void onTabOpenFromIntent(Tab tab) {
        super.onTabOpenFromIntent(tab);
        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "webapp");
        loadManifest(tab.getManifestPath());
    }

    /**
     * In case this activity and its tab are reused (the user has opened
     *  > 10 current web apps), we check that app launched is still within
     * the same host as the intent has set.
     * If it isn't, we reload the intent URL.
     */
    @Override
    protected void onTabSelectFromIntent(Tab tab) {
        super.onTabSelectFromIntent(tab);

        SafeIntent intent = new SafeIntent(getIntent());

        final String launchUrl = intent.getDataString();
        final String currentUrl = tab.getURL();
        final boolean isSameDomain = Uri.parse(currentUrl).getHost()
                .equals(Uri.parse(launchUrl).getHost());

        final String manifestPath;
        if (!isSameDomain) {
            Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.INTENT, "webapp");
            manifestPath = intent.getStringExtra(MANIFEST_PATH);
            tab.setManifestUrl(manifestPath);
            Tabs.getInstance().loadUrl(launchUrl);
        } else {
            manifestPath = tab.getManifestPath();
        }
        loadManifest(manifestPath);
    }

    private void loadManifest(String manifestPath) {
        if (TextUtils.isEmpty(manifestPath)) {
            Log.e(LOGTAG, "Missing manifest");
            return;
        }
        // The customisations defined in the manifest only work on Android API 21+
        if (AppConstants.Versions.preLollipop) {
            return;
        }

        try {
            final File manifestFile = new File(manifestPath);
            final JSONObject manifest = FileUtils.readJSONObjectFromFile(manifestFile);
            final JSONObject manifestField = manifest.getJSONObject("manifest");
            final Integer color = readColorFromManifest(manifestField);
            final String name = readNameFromManifest(manifestField);
            final Bitmap icon = readIconFromManifest(manifest);
            final ActivityManager.TaskDescription taskDescription = (color == null)
                    ? new ActivityManager.TaskDescription(name, icon)
                    : new ActivityManager.TaskDescription(name, icon, color);

            updateStatusBarColor(color);
            setTaskDescription(taskDescription);

        } catch (IOException | JSONException e) {
            Log.e(LOGTAG, "Failed to read manifest", e);
        }
    }

    private void updateStatusBarColor(final Integer themeColor) {
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
            .decodeDataURI(getContext(), iconStr);
        if (loadIconResult == null) {
            return null;
        }
        return loadIconResult.getBestBitmap(GeckoAppShell.getPreferredIconSize());
    }
}
