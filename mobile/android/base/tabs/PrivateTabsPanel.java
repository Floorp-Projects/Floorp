/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.Locale;

import org.mozilla.gecko.Locales;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tabs.TabsPanel.CloseAllPanelView;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

/**
 * A container that wraps the private tabs {@link android.widget.AdapterView} and empty
 * {@link android.view.View} to manage both of their visibility states by changing the visibility of
 * this container as calling {@link android.widget.AdapterView#setVisibility} does not affect the
 * empty View's visibility.
 */
class PrivateTabsPanel extends FrameLayout implements CloseAllPanelView {
    private TabsPanel tabsPanel;

    private final TabsLayout tabsLayout;

    public PrivateTabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.private_tabs_panel, this);
        tabsLayout = (TabsLayout) findViewById(R.id.private_tabs_layout);

        final LinearLayout emptyTabsFrame = (LinearLayout) findViewById(R.id.private_tabs_empty);
        tabsLayout.setEmptyView(emptyTabsFrame);

        final View learnMore = findViewById(R.id.private_tabs_learn_more);
        learnMore.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final String locale = Locales.getLanguageTag(Locale.getDefault());
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
