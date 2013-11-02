/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.widget.IconTabWidget;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

public class TabsPanel extends LinearLayout
                       implements LightweightTheme.OnChangeListener,
                                  IconTabWidget.OnTabChangedListener {
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
        public boolean shouldExpand();
    }

    public static interface TabsLayoutChangeListener {
        public void onTabsLayoutChange(int width, int height);
    }

    private Context mContext;
    private final GeckoApp mActivity;
    private final LightweightTheme mTheme;
    private RelativeLayout mHeader;
    private TabsListContainer mTabsContainer;
    private PanelView mPanel;
    private PanelView mPanelNormal;
    private PanelView mPanelPrivate;
    private PanelView mPanelRemote;
    private RelativeLayout mFooter;
    private TabsLayoutChangeListener mLayoutChangeListener;

    private IconTabWidget mTabWidget;
    private static ImageButton mAddTab;

    private Panel mCurrentPanel;
    private boolean mIsSideBar;
    private boolean mVisible;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (GeckoApp) context;
        mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

        setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                      LinearLayout.LayoutParams.FILL_PARENT));
        setOrientation(LinearLayout.VERTICAL);

        mCurrentPanel = Panel.NORMAL_TABS;
        mVisible = false;

        mIsSideBar = false;

        LayoutInflater.from(context).inflate(R.layout.tabs_panel, this);
        initialize();
    }

    private void initialize() {
        mHeader = (RelativeLayout) findViewById(R.id.tabs_panel_header);
        mTabsContainer = (TabsListContainer) findViewById(R.id.tabs_container);

        mPanelNormal = (TabsTray) findViewById(R.id.normal_tabs);
        mPanelNormal.setTabsPanel(this);

        mPanelPrivate = (TabsTray) findViewById(R.id.private_tabs);
        mPanelPrivate.setTabsPanel(this);

        mPanelRemote = (RemoteTabs) findViewById(R.id.synced_tabs);
        mPanelRemote.setTabsPanel(this);

        mFooter = (RelativeLayout) findViewById(R.id.tabs_panel_footer);

        mAddTab = (ImageButton) findViewById(R.id.add_tab);
        mAddTab.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                TabsPanel.this.addTab();
            }
        });

        mTabWidget = (IconTabWidget) findViewById(R.id.tab_widget);

        mTabWidget.addTab(R.drawable.tabs_normal, R.string.tabs_normal);
        mTabWidget.addTab(R.drawable.tabs_private, R.string.tabs_private);

        if (!GeckoProfile.get(mContext).inGuestMode()) {
            mTabWidget.addTab(R.drawable.tabs_synced, R.string.tabs_synced);
        }

        mTabWidget.setTabSelectionListener(this);
    }

    public void addTab() {
        if (mCurrentPanel == Panel.NORMAL_TABS)
           mActivity.addTab();
        else
           mActivity.addPrivateTab();

        mActivity.autoHideTabs();
    }

    @Override
    public void onTabChanged(int index) {
        if (index == 0)
            show(Panel.NORMAL_TABS, false);
        else if (index == 1)
            show(Panel.PRIVATE_TABS, false);
        else
            show(Panel.REMOTE_TABS, false);
    }

    private static int getTabContainerHeight(TabsListContainer listContainer) {
        Resources resources = listContainer.getContext().getResources();

        PanelView panelView = listContainer.getCurrentPanelView();
        if (panelView != null && !panelView.shouldExpand()) {
            return resources.getDimensionPixelSize(R.dimen.tabs_tray_horizontal_height);
        }

        int actionBarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height);
        int screenHeight = resources.getDisplayMetrics().heightPixels;

        Rect windowRect = new Rect();
        listContainer.getWindowVisibleDisplayFrame(windowRect);
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
        mTheme.addListener(this);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mTheme.removeListener(this);
    }
    
    @Override
    public void onLightweightThemeChanged() {
        final int background = getResources().getColor(R.color.background_tabs);
        final LightweightThemeDrawable drawable = mTheme.getColorDrawable(this, background, true);
        if (drawable == null)
            return;

        drawable.setAlpha(34, 0);
        setBackgroundDrawable(drawable);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundColor(getContext().getResources().getColor(R.color.background_tabs));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        onLightweightThemeChanged();
    }

    // Tabs List Container holds the ListView
    public static class TabsListContainer extends FrameLayout {
        public TabsListContainer(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public PanelView getCurrentPanelView() {
            final int childCount = getChildCount();
            for (int i = 0; i < childCount; i++) {
                View child = getChildAt(i);
                if (!(child instanceof PanelView))
                    continue;

                if (child.getVisibility() == View.VISIBLE)
                    return (PanelView) child;
            }

            return null;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            if (!GeckoAppShell.getGeckoInterface().hasTabsSideBar()) {
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
        private final LightweightTheme mTheme;

        public TabsPanelToolbar(Context context, AttributeSet attrs) {
            super(context, attrs);
            mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

            setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.FILL_PARENT,
                                                          (int) context.getResources().getDimension(R.dimen.browser_toolbar_height)));

            setOrientation(LinearLayout.HORIZONTAL);
        }

        @Override
        public void onAttachedToWindow() {
            super.onAttachedToWindow();
            mTheme.addListener(this);
        }

        @Override
        public void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            mTheme.removeListener(this);
        }
    
        @Override
        public void onLightweightThemeChanged() {
            final int background = getResources().getColor(R.color.background_tabs);
            final LightweightThemeDrawable drawable = mTheme.getColorDrawable(this, background);
            if (drawable == null)
                return;

            drawable.setAlpha(34, 34);
            setBackgroundDrawable(drawable);
        }

        @Override
        public void onLightweightThemeReset() {
            setBackgroundColor(getContext().getResources().getColor(R.color.background_tabs));
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            onLightweightThemeChanged();
        }
    }

    public void show(Panel panel) {
        show(panel, true);
    }

    public void show(Panel panel, boolean shouldResize) {
        if (!isShown())
            setVisibility(View.VISIBLE);

        if (mPanel != null) {
            // Hide the old panel.
            mPanel.hide();
        }

        final boolean showAnimation = !mVisible;
        mVisible = true;
        mCurrentPanel = panel;

        int index = panel.ordinal();
        mTabWidget.setCurrentTab(index);

        if (index == 0) {
            mPanel = mPanelNormal;
        } else if (index == 1) {
            mPanel = mPanelPrivate;
        } else {
            mPanel = mPanelRemote;
        }

        mPanel.show();

        if (mCurrentPanel == Panel.REMOTE_TABS) {
            if (mFooter != null)
                mFooter.setVisibility(View.GONE);

            mAddTab.setVisibility(View.INVISIBLE);
        } else {
            if (mFooter != null)
                mFooter.setVisibility(View.VISIBLE);

            mAddTab.setVisibility(View.VISIBLE);
            mAddTab.setImageLevel(index);
        }

        if (shouldResize) {
            if (isSideBar()) {
                if (showAnimation)
                    dispatchLayoutChange(getWidth(), getHeight());
            } else {
                int actionBarHeight = mContext.getResources().getDimensionPixelSize(R.dimen.browser_toolbar_height);
                int height = actionBarHeight + getTabContainerHeight(mTabsContainer);
                dispatchLayoutChange(getWidth(), height);
            }
        }
    }

    public void hide() {
        if (mVisible) {
            mVisible = false;
            dispatchLayoutChange(0, 0);
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

    public void setIsSideBar(boolean isSideBar) {
        mIsSideBar = isSideBar;
    }

    public Panel getCurrentPanel() {
        return mCurrentPanel;
    }

    public void prepareTabsAnimation(PropertyAnimator animator) {
        // Not worth doing this on pre-Honeycomb without proper
        // hardware accelerated animations.
        if (Build.VERSION.SDK_INT < 11) {
            return;
        }

        final Resources resources = getContext().getResources();
        final int toolbarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height);
        final int tabsPanelWidth = getWidth();

        if (mVisible) {
            if (mIsSideBar) {
                ViewHelper.setTranslationX(mHeader, -tabsPanelWidth);
            } else {
                ViewHelper.setTranslationY(mHeader, -toolbarHeight);
            }

            if (mIsSideBar) {
                ViewHelper.setTranslationX(mTabsContainer, -tabsPanelWidth);
            } else {
                ViewHelper.setTranslationY(mTabsContainer, -toolbarHeight);
                ViewHelper.setAlpha(mTabsContainer, 0);
            }

            // The footer view is only present on the sidebar
            if (mIsSideBar) {
                ViewHelper.setTranslationX(mFooter, -tabsPanelWidth);
            }
        }

        if (mIsSideBar) {
            final int translationX = (mVisible ? 0 : -tabsPanelWidth);

            animator.attach(mTabsContainer,
                            PropertyAnimator.Property.TRANSLATION_X,
                            translationX);
            animator.attach(mHeader,
                            PropertyAnimator.Property.TRANSLATION_X,
                            translationX);
            animator.attach(mFooter,
                            PropertyAnimator.Property.TRANSLATION_X,
                            translationX);
        } else {
            final int translationY = (mVisible ? 0 : -toolbarHeight);

            animator.attach(mTabsContainer,
                            PropertyAnimator.Property.ALPHA,
                            mVisible ? 1.0f : 0.0f);
            animator.attach(mTabsContainer,
                            PropertyAnimator.Property.TRANSLATION_Y,
                            translationY);
            animator.attach(mHeader,
                            PropertyAnimator.Property.TRANSLATION_Y,
                            translationY);
        }

        mHeader.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        mTabsContainer.setLayerType(View.LAYER_TYPE_HARDWARE, null);
    }

    public void finishTabsAnimation() {
        if (Build.VERSION.SDK_INT < 11) {
            return;
        }

        mHeader.setLayerType(View.LAYER_TYPE_NONE, null);
        mTabsContainer.setLayerType(View.LAYER_TYPE_NONE, null);
    }

    public void setTabsLayoutChangeListener(TabsLayoutChangeListener listener) {
        mLayoutChangeListener = listener;
    }

    private void dispatchLayoutChange(int width, int height) {
        if (mLayoutChangeListener != null)
            mLayoutChangeListener.onTabsLayoutChange(width, height);
    }
}
