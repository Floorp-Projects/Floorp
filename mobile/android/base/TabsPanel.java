/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.AdapterView;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.Spinner;

public class TabsPanel extends LinearLayout
                       implements GeckoPopupMenu.OnMenuItemClickListener,
                                  LightweightTheme.OnChangeListener,
                                  AdapterView.OnItemSelectedListener {
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
    private PanelView mPanelNormal;
    private PanelView mPanelPrivate;
    private PanelView mPanelRemote;
    private LinearLayout mFooter;
    private TabsLayoutChangeListener mLayoutChangeListener;

    private static ImageButton mMenuButton;
    private static ImageButton mAddTab;
    private Button mTabsMenuButton;
    private Spinner mTabsSpinner;

    private Panel mCurrentPanel;
    private boolean mIsSideBar;
    private boolean mVisible;
    private boolean mInflated;

    private GeckoPopupMenu mTabsPopupMenu;
    private Menu mTabsMenu;

    private GeckoPopupMenu mPopupMenu;
    private Menu mMenu;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (GeckoApp) context;

        setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                      LinearLayout.LayoutParams.FILL_PARENT));
        setOrientation(LinearLayout.VERTICAL);

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
        mTabsPopupMenu.showArrowToAnchor(false);
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

    void initialize() {
        mPanelNormal = (TabsTray) findViewById(R.id.normal_tabs);
        mPanelNormal.setTabsPanel(this);

        mPanelPrivate = (TabsTray) findViewById(R.id.private_tabs);
        mPanelPrivate.setTabsPanel(this);

        mPanelRemote = (RemoteTabs) findViewById(R.id.synced_tabs);
        mPanelRemote.setTabsPanel(this);

        mFooter = (LinearLayout) findViewById(R.id.tabs_panel_footer);

        mAddTab = (ImageButton) findViewById(R.id.add_tab);
        mAddTab.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                TabsPanel.this.addTab();
            }
        });

        mTabsSpinner = (Spinner) findViewById(R.id.tabs_menu);
        mTabsSpinner.setOnItemSelectedListener(this);

        mTabsMenuButton = (Button) findViewById(R.id.tabs_switcher_menu);
        mTabsPopupMenu.setAnchor(mTabsMenuButton);
        mTabsMenuButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                TabsPanel.this.openTabsSwitcherMenu();
            }
        });

        mMenuButton = (ImageButton) findViewById(R.id.menu);
        mMenuButton.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View view) {
                TabsPanel.this.openTabsMenu();
            }
        });

        mPopupMenu.setAnchor(mMenuButton);
    }

    public void addTab() {
        if (mCurrentPanel == Panel.NORMAL_TABS)
           mActivity.addTab();
        else
           mActivity.addPrivateTab();

        mActivity.autoHideTabs();
    }

    public void openTabsMenu() {
        if (mCurrentPanel == Panel.REMOTE_TABS)
            mMenu.findItem(R.id.close_all_tabs).setEnabled(false);
        else
            mMenu.findItem(R.id.close_all_tabs).setEnabled(true); 

        mPopupMenu.show();
    }

    public void openTabsSwitcherMenu() {
        mTabsPopupMenu.show();
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (!mVisible)
            return;

        Panel panel = TabsPanel.Panel.NORMAL_TABS;
        if (position == 1)
            panel = TabsPanel.Panel.PRIVATE_TABS;
        else if (position == 2)
            panel = TabsPanel.Panel.REMOTE_TABS;

        if (panel != mCurrentPanel)
            show(panel);
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.tabs_normal:
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

            case R.id.close_all_tabs:
                for (Tab tab : Tabs.getInstance().getTabsInOrder()) {
                    Tabs.getInstance().closeTab(tab);
                }
                autoHidePanel();
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
                int heightSpec = MeasureSpec.makeMeasureSpec(getTabContainerHeight(TabsListContainer.this), MeasureSpec.EXACTLY);
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
        mTabsSpinner.setSelection(index);

        if (index == 0) {
            mPanel = mPanelNormal;
            mTabsMenuButton.setText(R.string.tabs_normal);
        } else if (index == 1) {
            mPanel = mPanelPrivate;
            mTabsMenuButton.setText(R.string.tabs_private);
        } else {
            mPanel = mPanelRemote;
            mTabsMenuButton.setText(R.string.tabs_synced);
        }

        mPanel.show();

        if (mCurrentPanel == Panel.REMOTE_TABS) {
            if (mFooter != null)
                mFooter.setVisibility(View.GONE);

            mAddTab.setVisibility(View.INVISIBLE);
            mMenuButton.setVisibility(View.INVISIBLE);
        } else {
            if (mFooter != null)
                mFooter.setVisibility(View.VISIBLE);

            mAddTab.setVisibility(View.VISIBLE);
            mAddTab.setImageLevel(index);
            mMenuButton.setVisibility(View.VISIBLE);
        }

        if (isSideBar()) {
            if (showAnimation)
                dispatchLayoutChange(getWidth(), getHeight());
        } else {
            int actionBarHeight = mContext.getResources().getDimensionPixelSize(R.dimen.browser_toolbar_height);
            int height = actionBarHeight + getTabContainerHeight(this);
            dispatchLayoutChange(getWidth(), height);
        }
    }

    public void hide() {
        if (mVisible) {
            mVisible = false;
            mPopupMenu.dismiss();
            dispatchLayoutChange(0, 0);

            if (mPanel != null) {
                mPanel.hide();
                mPanel = null;
            }
        }
    }

    public void refresh() {
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
