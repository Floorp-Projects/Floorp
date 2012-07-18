/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.mozilla.gecko.sync.setup.SyncAccounts;

public class TabsPanel extends LinearLayout {
    private static final String LOGTAG = "GeckoTabsPanel";

    public static enum Panel {
        LOCAL_TABS,
        REMOTE_TABS
    }

    public static interface PanelView {
        public ViewGroup getLayout();
        public void show();
        public void hide();
    }

    public static interface TabsLayoutChangeListener {
        public void onTabsLayoutChange(int width, int height);
    }

    private Context mContext;
    private PanelView mPanel;
    private TabsPanelToolbar mToolbar;
    private TabsListContainer mListContainer;
    private TabsLayoutChangeListener mLayoutChangeListener;

    private static ImageButton mRemoteTabs;
    private TextView mTitle;

    private Panel mCurrentPanel;
    private boolean mVisible;

    private static final int REMOTE_TABS_HIDDEN = 1;
    private static final int REMOTE_TABS_SHOWN = 2;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        setOrientation(LinearLayout.VERTICAL);
        LayoutInflater.from(context).inflate(R.layout.tabs_panel, this);

        mCurrentPanel = Panel.LOCAL_TABS;
        mVisible = false;

        mToolbar = (TabsPanelToolbar) findViewById(R.id.toolbar);
        mListContainer = (TabsListContainer) findViewById(R.id.list_container);

        initToolbar();
    }

    void initToolbar() {
        mTitle = (TextView) mToolbar.findViewById(R.id.title);
        ImageButton addTab = (ImageButton) mToolbar.findViewById(R.id.add_tab);
        addTab.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                GeckoApp.mAppContext.addTab();
                GeckoApp.mAppContext.autoHideTabs();
            }
        });

        mRemoteTabs = (ImageButton) mToolbar.findViewById(R.id.remote_tabs);
        mRemoteTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                if (mRemoteTabs.getDrawable().getLevel() == REMOTE_TABS_SHOWN)
                    GeckoApp.mAppContext.showLocalTabs();
                else
                    GeckoApp.mAppContext.showRemoteTabs();
            }
        });
    }

    // Tabs List Container holds the ListView
    public static class TabsListContainer extends LinearLayout {
        public TabsListContainer(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (!GeckoApp.mAppContext.isTablet()) {
                DisplayMetrics metrics = new DisplayMetrics();
                GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);

                int height = (int) (0.5 * metrics.heightPixels);
                int heightSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
                super.onMeasure(widthMeasureSpec, heightSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }
    }

    // Tabs Panel Toolbar contains the Buttons
    public static class TabsPanelToolbar extends RelativeLayout {
        public TabsPanelToolbar(Context context, AttributeSet attrs) {
            super(context, attrs);

            setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                          (int) context.getResources().getDimension(R.dimen.browser_toolbar_height)));

            int panelToolbarRes;

            if (GeckoApp.mAppContext.hasPermanentMenuKey())
                panelToolbarRes = R.layout.tabs_panel_toolbar_menu;
            else
                panelToolbarRes = R.layout.tabs_panel_toolbar;

            LayoutInflater.from(context).inflate(panelToolbarRes, this);
        }
    }

    public void show(Panel panel) {
        if (mPanel != null) {
            // Remove the old panel.
            mPanel.hide();
            mListContainer.removeAllViews();
        }

        mVisible = true;
        mCurrentPanel = panel;

        if (panel == Panel.LOCAL_TABS) {
            mPanel = new TabsTray(mContext, null);
            mTitle.setText("");
            mRemoteTabs.setImageLevel(REMOTE_TABS_HIDDEN);
        } else {
            mPanel = new RemoteTabs(mContext, null);
            mTitle.setText(R.string.remote_tabs);
            mRemoteTabs.setVisibility(View.VISIBLE);
            mRemoteTabs.setImageLevel(REMOTE_TABS_SHOWN);
        }

        mPanel.show();
        mListContainer.addView(mPanel.getLayout());

        if (GeckoApp.mAppContext.isTablet()) {
            dispatchLayoutChange(getWidth(), getHeight());
        } else {
            int actionBarHeight = (int) (mContext.getResources().getDimension(R.dimen.browser_toolbar_height));

            // TabsListContainer takes time to resize on rotation.
            // It's better to add 50% of the screen-size and dispatch it as height.
            DisplayMetrics metrics = new DisplayMetrics();
            GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getMetrics(metrics);
            int listHeight = (int) (0.5 * metrics.heightPixels);

            int height = actionBarHeight + listHeight; 
            dispatchLayoutChange(getWidth(), height);
        }

        // If Sync is set up, query the database for remote clients.
        final Context context = mContext;
        new SyncAccounts.AccountsExistTask() {
            @Override
            protected void onPostExecute(Boolean result) {
                if (!result.booleanValue()) {
                    return;
                }
                TabsAccessor.areClientsAvailable(context, new TabsAccessor.OnClientsAvailableListener() {
                    @Override
                    public void areAvailable(boolean available) {
                        final int visibility = available ? View.VISIBLE : View.GONE;
                        mRemoteTabs.setVisibility(visibility);
                    }
                });
            }
        }.execute(context);
    }

    public void hide() {
        if (mVisible) {
            mVisible = false;
            dispatchLayoutChange(0, 0);
        }
    }

    public void refresh() {
        mListContainer.forceLayout();

        int index = indexOfChild(mToolbar);
        removeViewAt(index);

        mToolbar = new TabsPanelToolbar(mContext, null);
        addView(mToolbar, index);
        initToolbar();

        if (mVisible)
            show(mCurrentPanel);
    }

    @Override
    public boolean isShown() {
        return mVisible;
    }

    public void setTabsLayoutChangeListener(TabsLayoutChangeListener listener) {
        mLayoutChangeListener = listener;
    }

    private void dispatchLayoutChange(int width, int height) {
        if (mLayoutChangeListener != null)
            mLayoutChangeListener.onTabsLayoutChange(width, height);
    }
}
