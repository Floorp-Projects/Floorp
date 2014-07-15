/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabspanel;

import java.util.Locale;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tabspanel.TabsPanel.CloseAllPanelView;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ScrollView;

/**
 * A container that wraps the private tabs {@link android.widget.AdapterView} and empty
 * {@link android.view.View} to manage both of their visibility states by changing the visibility of
 * this container as calling {@link android.widget.AdapterView#setVisibility} does not affect the
 * empty View's visibility.
 */
class PrivateTabsPanel extends ScrollView implements CloseAllPanelView {
    private TabsPanel tabsPanel;
    private TabsTray tabsTray;

    public PrivateTabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.private_tabs_panel, this);
        tabsTray = (TabsTray) findViewById(R.id.private_tabs_tray);

        final View emptyView = findViewById(R.id.private_tabs_empty);
        tabsTray.setEmptyView(emptyView);

        final View learnMore = findViewById(R.id.private_tabs_learn_more);
        learnMore.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final String locale = BrowserLocaleManager.getLanguageTag(Locale.getDefault());
                final String url =
                        getResources().getString(R.string.private_tabs_panel_learn_more_link, locale);
                Tabs.getInstance().loadUrlInTab(url);
                if (tabsPanel != null) {
                    tabsPanel.autoHidePanel();
                }
            }
        });
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
        tabsTray.setTabsPanel(panel);
    }

    @Override
    public void show() {
        tabsTray.show();
        setVisibility(View.VISIBLE);
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        tabsTray.hide();
    }

    @Override
    public boolean shouldExpand() {
        return tabsTray.shouldExpand();
    }

    @Override
    public void closeAll() {
        tabsTray.closeAll();
    }
}
