/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.ArrayList;

import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.tabs.TabsPanel.TabsLayout;
import org.mozilla.gecko.Tabs;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.View;
import android.widget.GridView;
import android.view.ViewGroup;
import android.widget.Button;

/**
 * A tabs layout implementation for the tablet redesign (bug 1014156).
 * Expected to replace TabsListLayout once complete.
 */

class TabsGridLayout extends GridView
                     implements TabsLayout,
                                Tabs.OnTabsChangedListener {
    private static final String LOGTAG = "Gecko" + TabsGridLayout.class.getSimpleName();

    private final Context mContext;
    private TabsPanel mTabsPanel;

    final private boolean mIsPrivate;

    private final TabsLayoutAdapter mTabsAdapter;

    public TabsGridLayout(Context context, AttributeSet attrs) {
        super(context, attrs, R.attr.tabGridLayoutViewStyle);
        mContext = context;

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsTray);
        mIsPrivate = (a.getInt(R.styleable.TabsTray_tabs, 0x0) == 1);
        a.recycle();

        mTabsAdapter = new TabsGridLayoutAdapter(mContext);
        setAdapter(mTabsAdapter);

        setRecyclerListener(new RecyclerListener() {
            @Override
            public void onMovedToScrapHeap(View view) {
                TabsLayoutItemView item = (TabsLayoutItemView) view;
                item.setThumbnail(null);
            }
        });
    }

    private class TabsGridLayoutAdapter extends TabsLayoutAdapter {

        final private Button.OnClickListener mCloseClickListener;
        final private View.OnClickListener mSelectClickListener;

        public TabsGridLayoutAdapter (Context context) {
            super(context);

            mCloseClickListener = new Button.OnClickListener() {
                @Override
                public void onClick(View v) {
                    TabsLayoutItemView itemView = (TabsLayoutItemView) v.getTag();
                    Tab tab = Tabs.getInstance().getTab(itemView.getTabId());
                    Tabs.getInstance().closeTab(tab);
                }
            };

            mSelectClickListener = new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    TabsLayoutItemView tab = (TabsLayoutItemView) v;
                    Tabs.getInstance().selectTab(tab.getTabId());
                    autoHidePanel();
                }
            };
        }

        @Override
        TabsLayoutItemView newView(int position, ViewGroup parent) {
            final TabsLayoutItemView item = super.newView(position, parent);
            item.setOnClickListener(mSelectClickListener);
            item.setCloseOnClickListener(mCloseClickListener);
            return item;
        }

        @Override
        public void bindView(TabsLayoutItemView view, Tab tab) {
            super.bindView(view, tab);

            // If we're recycling this view, there's a chance it was transformed during
            // the close animation. Remove any of those properties.
            resetTransforms(view);
        }
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        mTabsPanel = panel;
    }

    @Override
    public void show() {
        setVisibility(View.VISIBLE);
        Tabs.getInstance().refreshThumbnails();
        Tabs.registerOnTabsChangedListener(this);
        refreshTabsData();
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        Tabs.unregisterOnTabsChangedListener(this);
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Tab:Screenshot:Cancel",""));
        mTabsAdapter.clear();
    }

    @Override
    public boolean shouldExpand() {
        return true;
    }

    private void autoHidePanel() {
        mTabsPanel.autoHidePanel();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        switch (msg) {
            case ADDED:
                // Refresh the list to make sure the new tab is added in the right position.
                refreshTabsData();
                break;

            case CLOSED:
               if (tab.isPrivate() == mIsPrivate && mTabsAdapter.getCount() > 0) {
                   if (mTabsAdapter.removeTab(tab)) {
                       int selected = mTabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
                       updateSelectedStyle(selected);
                   }
               }
               break;

            case SELECTED:
                // Update the selected position, then fall through...
                updateSelectedPosition();
            case UNSELECTED:
                // We just need to update the style for the unselected tab...
            case THUMBNAIL:
            case TITLE:
            case RECORDING_CHANGE:
                View view = getChildAt(mTabsAdapter.getPositionForTab(tab) - getFirstVisiblePosition());
                if (view == null)
                    return;

                ((TabsLayoutItemView) view).assignValues(tab);
                break;
        }
    }

    // Updates the selected position in the list so that it will be scrolled to the right place.
    private void updateSelectedPosition() {
        int selected = mTabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
        updateSelectedStyle(selected);

        if (selected != -1) {
            setSelection(selected);
        }
    }

    /**
     * Updates the selected/unselected style for the tabs.
     *
     * @param selected position of the selected tab
     */
    private void updateSelectedStyle(int selected) {
        for (int i = 0; i < mTabsAdapter.getCount(); i++) {
            setItemChecked(i, (i == selected));
        }
    }

    private void refreshTabsData() {
        // Store a different copy of the tabs, so that we don't have to worry about
        // accidentally updating it on the wrong thread.
        ArrayList<Tab> tabData = new ArrayList<>();

        Iterable<Tab> allTabs = Tabs.getInstance().getTabsInOrder();
        for (Tab tab : allTabs) {
            if (tab.isPrivate() == mIsPrivate)
                tabData.add(tab);
        }

        mTabsAdapter.setTabs(tabData);
        updateSelectedPosition();
    }

    private void resetTransforms(View view) {
        ViewHelper.setAlpha(view, 1);
        ViewHelper.setTranslationX(view, 0);
    }

    @Override
    public void closeAll() {

        autoHidePanel();

        if (getChildCount() == 0) {
            return;
        }

        final Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
        for (Tab tab : tabs) {
            // In the normal panel we want to close all tabs (both private and normal),
            // but in the private panel we only want to close private tabs.
            if (!mIsPrivate || tab.isPrivate()) {
                Tabs.getInstance().closeTab(tab, false);
            }
        }
    }
}
