/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.skin.SkinConfig;
import org.mozilla.gecko.widget.RecyclerViewClickSupport;

import android.content.Context;
import android.content.res.TypedArray;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;

import java.util.ArrayList;

public abstract class TabsLayout extends RecyclerView
        implements TabsPanel.TabsLayout,
        Tabs.OnTabsChangedListener,
        RecyclerViewClickSupport.OnItemClickListener,
        TabsTouchHelperCallback.DismissListener,
        TabsTouchHelperCallback.DragListener {

    private static final String LOGTAG = "Gecko" + TabsLayout.class.getSimpleName();

    private final boolean isPrivate;
    private TabsPanel tabsPanel;
    private final TabsLayoutAdapter tabsAdapter;
    private View emptyView;

    public TabsLayout(Context context, AttributeSet attrs, int itemViewLayoutResId) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsLayout);
        isPrivate = (a.getInt(R.styleable.TabsLayout_tabs, 0x0) == 1);
        a.recycle();

        tabsAdapter = new TabsLayoutAdapter(context, itemViewLayoutResId, isPrivate,
                /* close on click listener */
                new Button.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        // The view here is the close button, which has a reference
                        // to the parent TabsLayoutItemView in its tag, hence the getTag() call.
                        TabsLayoutItemView itemView = (TabsLayoutItemView) v.getTag();
                        closeTab(itemView);
                    }
                });
        setAdapter(tabsAdapter);

        RecyclerViewClickSupport.addTo(this).setOnItemClickListener(this);

        setRecyclerListener(new RecyclerListener() {
            @Override
            public void onViewRecycled(RecyclerView.ViewHolder holder) {
                final TabsLayoutItemView itemView = (TabsLayoutItemView) holder.itemView;
                itemView.setThumbnail(null);
                itemView.setCloseVisible(true);
            }
        });
    }

    @Override
    public void setTabsPanel(TabsPanel panel) {
        tabsPanel = panel;
    }

    @Override
    public void show() {
        if (SkinConfig.isPhoton()) {
            final boolean hasTabs = (tabsAdapter.getItemCount() > 0);
            setVisibility(hasTabs ? VISIBLE : GONE);

            if (emptyView != null) {
                emptyView.setVisibility(hasTabs ? GONE : VISIBLE);
            }
        } else {
            setVisibility(View.VISIBLE);
        }

        Tabs.getInstance().refreshThumbnails();
        Tabs.registerOnTabsChangedListener(this);
        refreshTabsData();
    }

    @Override
    public void hide() {
        setVisibility(View.GONE);
        Tabs.unregisterOnTabsChangedListener(this);
        tabsAdapter.clear();

        if (emptyView != null) {
            emptyView.setVisibility(VISIBLE);
        }
    }

    @Override
    public boolean shouldExpand() {
        return true;
    }

    protected void autoHidePanel() {
        tabsPanel.autoHidePanel();
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        switch (msg) {
            case ADDED:
                int tabIndex = Integer.parseInt(data);
                tabIndex = tabIndex == Tabs.NEW_LAST_INDEX ? tabsAdapter.getItemCount() : tabIndex;
                tabsAdapter.notifyTabInserted(tab, tabIndex);
                if (addAtIndexRequiresScroll(tabIndex)) {
                    // (The SELECTED tab is updated *after* this call to ADDED, so don't just call
                    // scrollSelectedTabToTopOfTray().)
                    scrollToPosition(tabIndex);
                }
                break;

            case CLOSED:
                if (tab.isPrivate() == isPrivate && tabsAdapter.getItemCount() > 0) {
                    tabsAdapter.removeTab(tab);
                }
                break;

            case SELECTED:
            case UNSELECTED:
            case THUMBNAIL:
            case TITLE:
            case RECORDING_CHANGE:
            case AUDIO_PLAYING_CHANGE:
                tabsAdapter.notifyTabChanged(tab);
                break;
        }
    }

    /**
     * Addition of a tab at selected positions (dependent on LayoutManager) can result in a tab
     * being added out of view - return true if {@code index} is such a position.  This should be
     * called only after the add has occurred.
     */
    abstract protected boolean addAtIndexRequiresScroll(int index);

    protected int getSelectedAdapterPosition() {
        return tabsAdapter.getPositionForTab(Tabs.getInstance().getSelectedTab());
    }

    @Override
    public void onItemClicked(RecyclerView recyclerView, int position, View v) {
        final TabsLayoutItemView item = (TabsLayoutItemView) v;
        final int tabId = item.getTabId();
        final Tab tab = Tabs.getInstance().selectTab(tabId);
        if (tab == null) {
            // The tab that was clicked no longer exists in the tabs list (which can happen if you
            // tap on a tab while its remove animation is running), so ignore the click.
            return;
        }

        autoHidePanel();
        Tabs.getInstance().notifyListeners(tab, Tabs.TabEvents.OPENED_FROM_TABS_TRAY);
    }

    @Override
    public void onItemDismiss(View view) {
        closeTab(view);
    }

    @Override
    public boolean onItemMove(int fromPosition, int toPosition) {
        return tabsAdapter.moveTab(fromPosition, toPosition);
    }

    /**
     * Scroll the selected tab to the top of the tray.
     * One of the motivations for scrolling to the top is so that, as often as possible, if we open
     * a background tab from the selected tab, when we return to the tabs tray the new tab will be
     * visible for selecting without requiring additional scrolling.
     */
    protected void scrollSelectedTabToTopOfTray() {
        final int selected = getSelectedAdapterPosition();
        if (selected != NO_POSITION) {
            ((LinearLayoutManager)getLayoutManager()).scrollToPositionWithOffset(selected, 0);
        }
    }

    private void refreshTabsData() {
        // Store a different copy of the tabs, so that we don't have to worry about
        // accidentally updating it on the wrong thread.
        final ArrayList<Tab> tabData = new ArrayList<>();
        final Iterable<Tab> allTabs = Tabs.getInstance().getTabsInOrder();

        for (final Tab tab : allTabs) {
            if (tab.isPrivate() == isPrivate) {
                tabData.add(tab);
            }
        }

        tabsAdapter.setTabs(tabData);
        scrollSelectedTabToTopOfTray();

        // Show empty view if we're in private panel and there's no private tabs.
        boolean hasTabs = !tabData.isEmpty();
        setVisibility(hasTabs ? VISIBLE : GONE);

        if (emptyView != null) {
            emptyView.setVisibility(hasTabs ? GONE : VISIBLE);
        }
    }

    private void closeTab(View view) {
        final TabsLayoutItemView itemView = (TabsLayoutItemView) view;
        final Tab tab = getTabForView(itemView);
        if (tab == null) {
            // We can be null here if this is the second closeTab call resulting from a sufficiently
            // fast double tap on the close tab button.
            return;
        }

        final boolean closingLastTab = tabsAdapter.getItemCount() == 1;
        Tabs.getInstance().closeTab(tab, true);
        if (closingLastTab) {
            autoHidePanel();
        }
    }

    protected void closeAllTabs() {
        final Iterable<Tab> tabs = Tabs.getInstance().getTabsInOrder();
        for (final Tab tab : tabs) {
            // In the normal panel we want to close all tabs (both private and normal),
            // but in the private panel we only want to close private tabs.
            if (!isPrivate || tab.isPrivate()) {
                Tabs.getInstance().closeTab(tab, false);
            }
        }
    }

    @Override
    public void onChildAttachedToWindow(View child) {
        // Make sure we reset any attributes that may have been animated in this child's previous
        // incarnation.
        child.setTranslationX(0);
        child.setTranslationY(0);
        child.setAlpha(1);
    }

    private Tab getTabForView(View view) {
        if (view == null) {
            return null;
        }
        return Tabs.getInstance().getTab(((TabsLayoutItemView) view).getTabId());
    }

    @Override
    public void setEmptyView(View emptyView) {
        this.emptyView = emptyView;
    }

    @Override
    abstract public void closeAll();
}
