/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.customtabs;

import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.ActionBar;
import android.support.v7.widget.Toolbar;
import android.text.TextUtils;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ColorUtil;
import org.mozilla.gecko.util.GeckoRequest;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import java.lang.reflect.Field;

import static android.support.customtabs.CustomTabsIntent.EXTRA_TOOLBAR_COLOR;

public class CustomTabsActivity extends GeckoApp implements Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "CustomTabsActivity";
    private static final String SAVED_TOOLBAR_COLOR = "SavedToolbarColor";
    private static final String SAVED_TOOLBAR_TITLE = "SavedToolbarTitle";
    private static final int NO_COLOR = -1;
    private Toolbar toolbar;

    private ActionBar actionBar;
    private int tabId = -1;
    private boolean useDomainTitle = true;

    private int toolbarColor;
    private String toolbarTitle;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState != null) {
            toolbarColor = savedInstanceState.getInt(SAVED_TOOLBAR_COLOR, NO_COLOR);
            toolbarTitle = savedInstanceState.getString(SAVED_TOOLBAR_TITLE, AppConstants.MOZ_APP_BASENAME);
        } else {
            toolbarColor = NO_COLOR;
            toolbarTitle = AppConstants.MOZ_APP_BASENAME;
        }

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        updateActionBarWithToolbar(toolbar);
        try {
            // Since we don't create the Toolbar's TextView ourselves, this seems
            // to be the only way of changing the ellipsize setting.
            Field f = toolbar.getClass().getDeclaredField("mTitleTextView");
            f.setAccessible(true);
            TextView textView = (TextView) f.get(toolbar);
            textView.setEllipsize(TextUtils.TruncateAt.START);
        } catch (Exception e) {
            // If we can't ellipsize at the start of the title, we shouldn't display the host
            // so as to avoid displaying a misleadingly truncated host.
            Log.w(LOGTAG, "Failed to get Toolbar TextView, using default title.");
            useDomainTitle = false;
        }
        actionBar = getSupportActionBar();
        actionBar.setTitle(toolbarTitle);
        updateToolbarColor(toolbar);

        toolbar.setNavigationOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final Tabs tabs = Tabs.getInstance();
                final Tab tab = tabs.getSelectedTab();
                tabs.closeTab(tab);
                finish();
            }
        });

        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Tabs.unregisterOnTabsChangedListener(this);
    }

    @Override
    public int getLayout() {
        return R.layout.customtabs_activity;
    }

    @Override
    protected void onDone() {
        finish();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        if (tab == null) {
            return;
        }

        if (tabId >= 0 && tab.getId() != tabId) {
            return;
        }

        if (msg == Tabs.TabEvents.LOCATION_CHANGE) {
            tabId = tab.getId();
            final Uri uri = Uri.parse(tab.getURL());
            String title = null;
            if (uri != null) {
                title = uri.getHost();
            }
            if (!useDomainTitle || title == null || title.isEmpty()) {
                toolbarTitle = AppConstants.MOZ_APP_BASENAME;
            } else {
                toolbarTitle = title;
            }
            actionBar.setTitle(toolbarTitle);
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putInt(SAVED_TOOLBAR_COLOR, toolbarColor);
        outState.putString(SAVED_TOOLBAR_TITLE, toolbarTitle);
    }

    @Override
    public void onResume() {
        if (lastSelectedTabId >= 0) {
            final Tabs tabs = Tabs.getInstance();
            final Tab tab =  tabs.getTab(lastSelectedTabId);
            if (tab == null) {
                finish();
            }
        }
        super.onResume();
    }

    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                finish();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void updateActionBarWithToolbar(final Toolbar toolbar) {
        setSupportActionBar(toolbar);
        final ActionBar ab = getSupportActionBar();
        if (ab != null) {
            ab.setDisplayHomeAsUpEnabled(true);
        }
    }

    private void updateToolbarColor(final Toolbar toolbar) {
        if (toolbarColor == NO_COLOR) {
            final int color = getIntent().getIntExtra(EXTRA_TOOLBAR_COLOR, NO_COLOR);
            if (color == NO_COLOR) {
                return;
            }
            toolbarColor = color;
        }

        final int titleTextColor = ColorUtil.getReadableTextColor(toolbarColor);

        toolbar.setBackgroundColor(toolbarColor);
        toolbar.setTitleTextColor(titleTextColor);
        final Window window = getWindow();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            window.setStatusBarColor(ColorUtil.darken(toolbarColor, 0.25));
        }
    }
}
