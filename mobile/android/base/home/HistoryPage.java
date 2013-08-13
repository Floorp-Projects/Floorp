/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.IconTabWidget;
import android.support.v4.app.Fragment;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;

import android.widget.ImageButton;

public class HistoryPage extends HomeFragment
                        implements IconTabWidget.OnTabChangedListener {
    // Logging tag name
    private static final String LOGTAG = "GeckoHistoryPage";
    private IconTabWidget mTabWidget;
    private int mSelectedTab;
    private boolean initializeVisitedPage;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_history_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mTabWidget = (IconTabWidget) view.findViewById(R.id.tab_icon_widget);

        mTabWidget.addTab(R.drawable.icon_most_visited, R.string.home_most_visited_title);
        mTabWidget.addTab(R.drawable.icon_most_recent, R.string.home_most_recent_title);
        mTabWidget.addTab(R.drawable.icon_last_tabs, R.string.home_last_tabs_title);

        mTabWidget.setTabSelectionListener(this);
        mTabWidget.setCurrentTab(mSelectedTab);

        loadIfVisible();
    }

    @Override
    public void load() {
        // Show most visited page as the initial page.
        // Since we detach/attach on config change, this prevents from replacing current fragment.
        if (!initializeVisitedPage) {
            showMostVisitedPage();
            initializeVisitedPage = true;
        }
    }

    @Override
    public void onTabChanged(int index) {
        if (index == mSelectedTab) {
            return;
        }

        if (index == 0) {
            showMostVisitedPage();
        } else if (index == 1) {
            showMostRecentPage();
        } else if (index == 2) {
            showLastTabsPage();
        }

        mTabWidget.setCurrentTab(index);
        mSelectedTab = index;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Rotation should detach and re-attach to use a different layout.
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    private void showSubPage(Fragment subPage) {
        getChildFragmentManager().beginTransaction()
                .addToBackStack(null).replace(R.id.visited_page_container, subPage)
                .commitAllowingStateLoss();
    }

    private void showMostVisitedPage() {
        final MostVisitedPage mostVisitedPage = MostVisitedPage.newInstance();
        showSubPage(mostVisitedPage);
    }

    private void showMostRecentPage() {
        final MostRecentPage mostRecentPage = MostRecentPage.newInstance();
        showSubPage(mostRecentPage);
    }

    private void showLastTabsPage() {
        final LastTabsPage lastTabsPage = LastTabsPage.newInstance();
        showSubPage(lastTabsPage);
    }
}
