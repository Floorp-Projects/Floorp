/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.skin.SkinConfig;
import org.mozilla.gecko.tabs.TabsPanel.CloseAllPanelView;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import static org.mozilla.gecko.Tabs.LOADURL_NEW_TAB;
import static org.mozilla.gecko.Tabs.LOADURL_PRIVATE;

/**
 * A container that wraps the private tabs {@link android.widget.AdapterView} and empty
 * {@link android.view.View} to manage both of their visibility states by changing the visibility of
 * this container as calling {@link android.widget.AdapterView#setVisibility} does not affect the
 * empty View's visibility.
 */
class PrivateTabsPanel extends RelativeLayout implements CloseAllPanelView {
    private final static String PRIVATE_BROWSING_URL = "https://support.mozilla.org/kb/private-browsing-firefox-android";

    private final TabsLayout tabsLayout;

    public PrivateTabsPanel(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.private_tabs_panel, this);
        tabsLayout = (TabsLayout) findViewById(R.id.private_tabs_layout);

        final View emptyTabsFrame = findViewById(R.id.private_tabs_empty);
        tabsLayout.setEmptyView(emptyTabsFrame);

        if (SkinConfig.isPhoton()) {
            final TextView learnMoreView = (TextView) findViewById(R.id.learn_more_link);
            learnMoreView.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.ACTIONBAR, "new_tab");

                    Tabs.getInstance().loadUrl(PRIVATE_BROWSING_URL, LOADURL_NEW_TAB | LOADURL_PRIVATE);

                    final GeckoApp geckoApp = (GeckoApp) context;
                    geckoApp.autoHideTabs();
                }
            });
        }
    }

    @Override
    public void setTabsPanel(final TabsPanel panel) {
        tabsLayout.setTabsPanel(panel);
    }

    @Override
    public void show() {
        tabsLayout.show();
        setVisibility(View.VISIBLE);
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        tabsLayout.hide();
    }

    @Override
    public boolean shouldExpand() {
        return tabsLayout.shouldExpand();
    }

    @Override
    public void closeAll() {
        tabsLayout.closeAll();
    }
}
