/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.sync.setup.SyncAccounts;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.LayerDrawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TabHost;
import android.widget.TabHost.TabSpec;
import android.widget.TabWidget;
import android.widget.TextView;

public class TabsPanel extends TabHost
                       implements GeckoPopupMenu.OnMenuItemClickListener,
                                  LightweightTheme.OnChangeListener {
    private static final String LOGTAG = "GeckoTabsPanel";

    public static enum Panel {
        NORMAL_TABS,
        PRIVATE_TABS,
        REMOTE_TABS
    }

    public static interface PanelView {
        public ViewGroup getLayout();
        public void setTabsPanel(TabsPanel panel);
        public void show();
        public void hide();
    }

    public static interface TabsLayoutChangeListener {
        public void onTabsLayoutChange(int width, int height);
    }

    private Context mContext;
    private GeckoApp mActivity;
    private PanelView mPanel;
    private TabsPanelToolbar mToolbar;
    private TabsLayoutChangeListener mLayoutChangeListener;

    private static ImageButton mMenuButton;
    private static ImageButton mAddTab;
    private TabWidget mTabWidget;
    private Button mTabsMenuButton;

    private Panel mCurrentPanel;
    private boolean mIsSideBar;
    private boolean mVisible;
    private boolean mInflated;

    private GeckoPopupMenu mPopupMenu;
    private Menu mMenu;

    private GeckoPopupMenu mTabsPopupMenu;
    private Menu mTabsMenu;

    private static final int REMOTE_TABS_HIDDEN = 1;
    private static final int REMOTE_TABS_SHOWN = 2;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (GeckoApp) context;

        mCurrentPanel = Panel.NORMAL_TABS;
        mVisible = false;

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabsPanel);
        mIsSideBar = a.getBoolean(R.styleable.TabsPanel_sidebar, false);
        a.recycle();

        mPopupMenu = new GeckoPopupMenu(context);
        mPopupMenu.inflate(R.menu.tabs_menu);
        mPopupMenu.setOnMenuItemClickListener(this);
        mMenu = mPopupMenu.getMenu();

        mTabsPopupMenu = new GeckoPopupMenu(context);
        mTabsPopupMenu.inflate(R.menu.tabs_switcher_menu);
        mTabsPopupMenu.setOnMenuItemClickListener(this);
        mTabsMenu = mTabsPopupMenu.getMenu();

        LayoutInflater.from(context).inflate(R.layout.tabs_panel, this);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        // HACK: Without this, the onFinishInflate is called twice
        // This issue is due to a bug when Android inflates a layout with a
        // parent. Fixed in Honeycomb
        if (mInflated)
            return;

        mInflated = true;

        initialize();
    }

    private void initialize() {
        // This should be called before adding any tabs
        // to the TabHost.
        setup();

        initToolbar();
        addTab(R.string.tabs_normal, R.id.normal_tabs);
        addTab(R.string.tabs_private, R.id.private_tabs);
        addTab(R.string.tabs_synced, R.id.synced_tabs);
    }

    private void addTab(int resId, int contentId) {
        String title = mContext.getString(resId);
        TabSpec spec = newTabSpec(title);
        GeckoTextView indicatorView = (GeckoTextView) LayoutInflater.from(mContext).inflate(R.layout.tabs_panel_indicator, null);
        indicatorView.setText(title);

        spec.setIndicator(indicatorView);
        spec.setContent(contentId);

        final int index = mTabWidget.getTabCount();
        PanelView panel = (PanelView) findViewById(contentId);
        panel.setTabsPanel(this);
        panel.show();

        addTab(spec);

        indicatorView.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Panel panel = Panel.NORMAL_TABS;
                if (index == 1)
                    panel = Panel.PRIVATE_TABS;
                else if (index == 2)
                    panel = Panel.REMOTE_TABS;

                TabsPanel.this.show(panel);
            }
        });
    }

    void initToolbar() {
        mToolbar = (TabsPanelToolbar) findViewById(R.id.toolbar);

        mTabWidget = (TabWidget) findViewById(android.R.id.tabs);

        mAddTab = (ImageButton) mToolbar.findViewById(R.id.add_tab);
        mAddTab.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                mActivity.addTab();
                mActivity.autoHideTabs();
            }
        });

        mTabsMenuButton = (Button) mToolbar.findViewById(R.id.tabs_menu);
        mTabsMenuButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                TabsPanel.this.openTabsSwitcherMenu();
            }
        });

        mTabsPopupMenu.setAnchor(mTabsMenuButton);

        mMenuButton = (ImageButton) mToolbar.findViewById(R.id.menu);
        mMenuButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                TabsPanel.this.openTabsMenu();
            }
        });

        mPopupMenu.setAnchor(mMenuButton);
    }

    public void openTabsMenu() {
        if (mCurrentPanel == Panel.REMOTE_TABS)
            mMenu.findItem(R.id.close_all_tabs).setEnabled(false);
        else
            mMenu.findItem(R.id.close_all_tabs).setEnabled(true); 

        mPopupMenu.show();

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
                        TabsPanel.this.enableRemoteTabs(available);
                    }
                });
            }
        }.execute(context);
    }

    public void enableRemoteTabs(boolean enable) {
        mMenu.findItem(R.id.synced_tabs).setEnabled(enable);
    }

    public void openTabsSwitcherMenu() {
        mTabsPopupMenu.show();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.tabs_normal:
                mTabsMenuButton.setText(R.string.tabs_normal);
                show(Panel.NORMAL_TABS);
                return true;

            case R.id.tabs_private:
                mTabsMenuButton.setText(R.string.tabs_private);
                show(Panel.PRIVATE_TABS);
                return true;

            case R.id.tabs_synced:
                mTabsMenuButton.setText(R.string.tabs_synced);
                show(Panel.REMOTE_TABS);
                return true;

            case R.id.synced_tabs:
                show(Panel.REMOTE_TABS);
                return true;

            case R.id.close_all_tabs:
                for (Tab tab : Tabs.getInstance().getTabsInOrder()) {
                    Tabs.getInstance().closeTab(tab);
                }
                return true;

            case R.id.new_tab:
            case R.id.new_private_tab:
                hide();
            // fall through
            default:
                return mActivity.onOptionsItemSelected(item);
        }
    }  

    private static int getTabContainerHeight(View view) {
        Context context = view.getContext();

        int actionBarHeight = context.getResources().getDimensionPixelSize(R.dimen.browser_toolbar_height);
        int screenHeight = context.getResources().getDisplayMetrics().heightPixels;

        Rect windowRect = new Rect();
        view.getWindowVisibleDisplayFrame(windowRect);
        int windowHeight = windowRect.bottom - windowRect.top;

        // The web content area should have at least 1.5x the height of the action bar.
        // The tabs panel shouldn't take less than 50% of the screen height and can take
        // up to 80% of the window height.
        return (int) Math.max(screenHeight * 0.5f,
                              Math.min(windowHeight - 2.5f * actionBarHeight, windowHeight * 0.8f) - actionBarHeight);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mActivity.getLightweightTheme().addListener(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mActivity.getLightweightTheme().removeListener(this);
    }
    
    @Override
    public void onLightweightThemeChanged() {
        LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getTextureDrawable(this, R.drawable.tabs_tray_bg_repeat, true);
        if (drawable == null)
            return;

        drawable.setAlpha(34, 0);
        setBackgroundDrawable(drawable);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundResource(R.drawable.tabs_tray_bg_repeat);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        onLightweightThemeChanged();
    }

    // Tabs List Container holds the ListView
    public static class TabsListContainer extends FrameLayout {
        private Context mContext;

        public TabsListContainer(Context context, AttributeSet attrs) {
            super(context, attrs);
            mContext = context;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (!GeckoApp.mAppContext.hasTabsSideBar()) {
                int heightSpec = MeasureSpec.makeMeasureSpec(getTabContainerHeight(this), MeasureSpec.EXACTLY);
                super.onMeasure(widthMeasureSpec, heightSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }
    }

    // Tabs Panel Toolbar contains the Buttons
    public static class TabsPanelToolbar extends LinearLayout 
                                         implements LightweightTheme.OnChangeListener {
        private BrowserApp mActivity;

        public TabsPanelToolbar(Context context, AttributeSet attrs) {
            super(context, attrs);
            mActivity = (BrowserApp) context;

            setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                          (int) context.getResources().getDimension(R.dimen.browser_toolbar_height)));

            setOrientation(LinearLayout.HORIZONTAL);

            LayoutInflater.from(context).inflate(R.layout.tabs_panel_toolbar_menu, this);
        }

        @Override
        public void onAttachedToWindow() {
            super.onAttachedToWindow();
            mActivity.getLightweightTheme().addListener(this);
        }

        @Override
        public void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            mActivity.getLightweightTheme().removeListener(this);
        }
    
        @Override
        public void onLightweightThemeChanged() {
            LightweightThemeDrawable drawable = mActivity.getLightweightTheme().getTextureDrawable(this, R.drawable.tabs_tray_bg_repeat);
            if (drawable == null)
                return;

            drawable.setAlpha(34, 34);
            setBackgroundDrawable(drawable);
        }

        @Override
        public void onLightweightThemeReset() {
            setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            onLightweightThemeChanged();
        }
    }

    public void show(Panel panel) {
        if (mPanel != null) {
            // Hide the old panel.
            mPanel.hide();
        }

        final boolean showAnimation = !mVisible;
        mVisible = true;
        mCurrentPanel = panel;

        int index = panel.ordinal();
        setCurrentTab(index);

        mPanel = (PanelView) getTabContentView().getChildAt(index);
        mPanel.show();

        if (isSideBar()) {
            if (showAnimation)
                dispatchLayoutChange(getWidth(), getHeight());
        } else {
            int actionBarHeight = mContext.getResources().getDimensionPixelSize(R.dimen.browser_toolbar_height);
            int height = actionBarHeight + getTabContainerHeight(getTabContentView());
            dispatchLayoutChange(getWidth(), height);
        }
    }

    public void hide() {
        if (mVisible) {
            mVisible = false;
            mPopupMenu.dismiss();
            dispatchLayoutChange(0, 0);

            mPanel.hide();
            mPanel = null;
        }
    }

    public void refresh() {
        clearAllTabs();
        removeAllViews();

        LayoutInflater.from(mContext).inflate(R.layout.tabs_panel, this);
        initialize();

        if (mVisible)
            show(mCurrentPanel);
    }

    public void autoHidePanel() {
        mActivity.autoHideTabs();
    }

    @Override
    public boolean isShown() {
        return mVisible;
    }

    public boolean isSideBar() {
        return mIsSideBar;
    }

    public void setTabsLayoutChangeListener(TabsLayoutChangeListener listener) {
        mLayoutChangeListener = listener;
    }

    private void dispatchLayoutChange(int width, int height) {
        if (mLayoutChangeListener != null)
            mLayoutChangeListener.onTabsLayoutChange(width, height);
    }
}
