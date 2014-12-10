/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.Locale;

import org.mozilla.gecko.Locales;
import org.mozilla.gecko.NewTabletUI;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.tabs.TabsPanel.CloseAllPanelView;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.Gravity;
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
    private final View emptyTabsHeader;
    private final LinearLayout emptyTabsFrame;

    private final int emptyTabsFrameWidth;
    private final int emptyTabsFrameVerticalOffset;

    public PrivateTabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);

        final Resources res = getResources();
        emptyTabsFrameVerticalOffset = res.getDimensionPixelOffset(R.dimen.browser_toolbar_height);
        emptyTabsFrameWidth =
                res.getDimensionPixelSize(R.dimen.new_tablet_private_tabs_panel_empty_width);

        LayoutInflater.from(context).inflate(R.layout.private_tabs_panel, this);
        tabsLayout = (TabsLayout) findViewById(R.id.private_tabs_layout);
        emptyTabsHeader = findViewById(R.id.private_tabs_empty_header);

        emptyTabsFrame = (LinearLayout) findViewById(R.id.private_tabs_empty);
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
        updateStyleForNewTablet();

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

    private void updateStyleForNewTablet() {
        if (!NewTabletUI.isEnabled(getContext())) {
            return;
        }

        // TODO: Move this to styles when removing old tablet.
        // Delete the emptyTabsFrame & Header class vars too.
        emptyTabsFrame.setOrientation(LinearLayout.VERTICAL);

        final FrameLayout.LayoutParams lp =
                (FrameLayout.LayoutParams) emptyTabsFrame.getLayoutParams();
        lp.width = getResources().getDimensionPixelSize(
                R.dimen.new_tablet_private_tabs_panel_empty_width);
        lp.height = LayoutParams.WRAP_CONTENT;

        // We want to center the content on the screen, not in the View,
        // so add padding to compensate for the header.
        lp.gravity = Gravity.CENTER;
        emptyTabsFrame.setPadding(0, 0, 0, emptyTabsFrameVerticalOffset);

        emptyTabsHeader.setVisibility(View.VISIBLE);
    }
}
