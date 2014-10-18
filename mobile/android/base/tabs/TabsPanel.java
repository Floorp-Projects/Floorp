/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoAppShell.AppStateListener;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.LightweightThemeDrawable;
import org.mozilla.gecko.NewTabletUI;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.widget.GeckoPopupMenu;
import org.mozilla.gecko.widget.IconTabWidget;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

public class TabsPanel extends LinearLayout
                       implements GeckoPopupMenu.OnMenuItemClickListener,
                                  LightweightTheme.OnChangeListener,
                                  IconTabWidget.OnTabChangedListener {
    private static final String LOGTAG = "Gecko" + TabsPanel.class.getSimpleName();

    public static enum Panel {
        NORMAL_TABS,
        PRIVATE_TABS,
        REMOTE_TABS
    }

    public static interface PanelView {
        public void setTabsPanel(TabsPanel panel);
        public void show();
        public void hide();
        public boolean shouldExpand();
    }

    public static interface CloseAllPanelView extends PanelView {
        public void closeAll();
    }

    public static interface TabsLayout extends CloseAllPanelView {
        public void setEmptyView(View view);
    }

    public static View createTabsLayout(final Context context, final AttributeSet attrs) {
        if (NewTabletUI.isEnabled(context)) {
            return new TabsGridLayout(context, attrs);
        } else {
            return new TabsListLayout(context, attrs);
        }
    }

    public static interface TabsLayoutChangeListener {
        public void onTabsLayoutChange(int width, int height);
    }

    private final Context mContext;
    private final GeckoApp mActivity;
    private final LightweightTheme mTheme;
    private RelativeLayout mHeader;
    private TabsLayoutContainer mTabsContainer;
    private PanelView mPanel;
    private PanelView mPanelNormal;
    private PanelView mPanelPrivate;
    private PanelView mPanelRemote;
    private RelativeLayout mFooter;
    private TabsLayoutChangeListener mLayoutChangeListener;
    private final AppStateListener mAppStateListener;

    private IconTabWidget mTabWidget;
    private static ImageButton mMenuButton;
    private static ImageButton mAddTab;

    private Panel mCurrentPanel;
    private boolean mIsSideBar;
    private boolean mVisible;
    private boolean mHeaderVisible;

    private final GeckoPopupMenu mPopupMenu;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (GeckoApp) context;
        mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

        setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                                                      LinearLayout.LayoutParams.MATCH_PARENT));
        setOrientation(LinearLayout.VERTICAL);

        mCurrentPanel = Panel.NORMAL_TABS;

        mPopupMenu = new GeckoPopupMenu(context);
        mPopupMenu.inflate(R.menu.tabs_menu);
        mPopupMenu.setOnMenuItemClickListener(this);

        inflateLayout(context);
        initialize();

        mAppStateListener = new AppStateListener() {
            @Override
            public void onResume() {
                if (mPanel == mPanelRemote) {
                    // Refresh the remote panel.
                    mPanelRemote.show();
                }
            }

            @Override
            public void onOrientationChanged() {
                // Remote panel is already refreshed by chrome refresh.
            }

            @Override
            public void onPause() {}
        };
    }


    private void inflateLayout(Context context) {
        if (NewTabletUI.isEnabled(context)) {
            LayoutInflater.from(context).inflate(R.layout.tabs_panel_default, this);
        } else {
            LayoutInflater.from(context).inflate(R.layout.tabs_panel, this);
        }
    }

    private void initialize() {
        mHeader = (RelativeLayout) findViewById(R.id.tabs_panel_header);
        mTabsContainer = (TabsLayoutContainer) findViewById(R.id.tabs_container);

        mPanelNormal = (PanelView) findViewById(R.id.normal_tabs);
        mPanelNormal.setTabsPanel(this);

        mPanelPrivate = (PanelView) findViewById(R.id.private_tabs_panel);
        mPanelPrivate.setTabsPanel(this);

        mPanelRemote = (PanelView) findViewById(R.id.remote_tabs);
        mPanelRemote.setTabsPanel(this);

        // Only applies to v11+ in landscape.
        // We ship a stub to avoid a compiler error when referencing the
        // ID, so we conditionalize here.
        if (Versions.feature11Plus) {
            mFooter = (RelativeLayout) findViewById(R.id.tabs_panel_footer);
        }

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

        if (RestrictedProfiles.isAllowed(RestrictedProfiles.Restriction.DISALLOW_MODIFY_ACCOUNTS)) {
            // The initial icon is not the animated icon, because on Android
            // 4.4.2, the animation starts immediately (and can start at other
            // unpredictable times). See Bug 1015974.
            mTabWidget.addTab(R.drawable.tabs_synced, R.string.tabs_synced);
        }

        mTabWidget.setTabSelectionListener(this);

        mMenuButton = (ImageButton) findViewById(R.id.menu);
        mMenuButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                showMenu();
            }
        });
    }

    public void showMenu() {
        if (mCurrentPanel == Panel.REMOTE_TABS) {
            return;
        }

        final Menu menu = mPopupMenu.getMenu();

        // Each panel has a "+" shortcut button, so don't show it for that panel.
        menu.findItem(R.id.new_tab).setVisible(mCurrentPanel != Panel.NORMAL_TABS);
        menu.findItem(R.id.new_private_tab).setVisible(mCurrentPanel != Panel.PRIVATE_TABS);

        // Only show "Clear * tabs" for current panel.
        menu.findItem(R.id.close_all_tabs).setVisible(mCurrentPanel == Panel.NORMAL_TABS);
        menu.findItem(R.id.close_private_tabs).setVisible(mCurrentPanel == Panel.PRIVATE_TABS);

        mPopupMenu.show();
    }

    private void addTab() {
        if (mCurrentPanel == Panel.NORMAL_TABS) {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.ACTIONBAR, "new_tab");
            mActivity.addTab();
        } else {
            Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.ACTIONBAR, "new_private_tab");
            mActivity.addPrivateTab();
        }

        mActivity.autoHideTabs();
    }

    @Override
    public void onTabChanged(int index) {
        if (index == 0) {
            show(Panel.NORMAL_TABS);
        } else if (index == 1) {
            show(Panel.PRIVATE_TABS);
        } else {
            show(Panel.REMOTE_TABS);
        }
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        final int itemId = item.getItemId();

        if (itemId == R.id.close_all_tabs) {
            if (mCurrentPanel == Panel.NORMAL_TABS) {
                final String extras = getResources().getResourceEntryName(itemId);
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, extras);

                // Disable the menu button so that the menu won't interfere with the tab close animation.
                mMenuButton.setEnabled(false);
                ((CloseAllPanelView) mPanelNormal).closeAll();
            } else {
                Log.e(LOGTAG, "Close all tabs menu item should only be visible for normal tabs panel");
            }
            return true;
        }

        if (itemId == R.id.close_private_tabs) {
            if (mCurrentPanel == Panel.PRIVATE_TABS) {
                final String extras = getResources().getResourceEntryName(itemId);
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.MENU, extras);

                ((CloseAllPanelView) mPanelPrivate).closeAll();
            } else {
                Log.e(LOGTAG, "Close private tabs menu item should only be visible for private tabs panel");
            }
            return true;
        }

        if (itemId == R.id.new_tab || itemId == R.id.new_private_tab) {
            hide();
        }

        return mActivity.onOptionsItemSelected(item);
    }

    private static int getTabContainerHeight(TabsLayoutContainer tabsContainer) {
        Resources resources = tabsContainer.getContext().getResources();

        PanelView panelView = tabsContainer.getCurrentPanelView();
        if (panelView != null && !panelView.shouldExpand()) {
            return resources.getDimensionPixelSize(R.dimen.tabs_layout_horizontal_height);
        }

        int actionBarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height);
        int screenHeight = resources.getDisplayMetrics().heightPixels;

        Rect windowRect = new Rect();
        tabsContainer.getWindowVisibleDisplayFrame(windowRect);
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
        mActivity.addAppStateListener(mAppStateListener);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mTheme.removeListener(this);
        mActivity.removeAppStateListener(mAppStateListener);
    }

    @Override
    @SuppressWarnings("deprecation") // setBackgroundDrawable deprecated by API level 16
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

    static class TabsLayoutContainer extends FrameLayout {
        public TabsLayoutContainer(Context context, AttributeSet attrs) {
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
                int heightSpec = MeasureSpec.makeMeasureSpec(getTabContainerHeight(TabsLayoutContainer.this), MeasureSpec.EXACTLY);
                super.onMeasure(widthMeasureSpec, heightSpec);
            } else {
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        }
    }

    // Tabs Panel Toolbar contains the Buttons
    static class TabsPanelToolbar extends LinearLayout
                                  implements LightweightTheme.OnChangeListener {
        private final LightweightTheme mTheme;

        public TabsPanelToolbar(Context context, AttributeSet attrs) {
            super(context, attrs);
            mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

            setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
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
        @SuppressWarnings("deprecation") // setBackgroundDrawable deprecated by API level 16
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

    public void show(Panel panelToShow) {
        if (!isShown())
            setVisibility(View.VISIBLE);

        if (mPanel != null) {
            // Hide the old panel.
            mPanel.hide();
        }

        final boolean showAnimation = !mVisible;
        mVisible = true;
        mCurrentPanel = panelToShow;

        int index = panelToShow.ordinal();
        mTabWidget.setCurrentTab(index);

        switch (panelToShow) {
            case NORMAL_TABS:
                mPanel = mPanelNormal;
                break;
            case PRIVATE_TABS:
                mPanel = mPanelPrivate;
                break;
            case REMOTE_TABS:
                mPanel = mPanelRemote;
                break;

            default:
                throw new IllegalArgumentException("Unknown panel type " + panelToShow);
        }
        mPanel.show();

        if (mCurrentPanel == Panel.REMOTE_TABS) {
            // The footer is only defined in the sidebar, for landscape v11+ views.
            if (mFooter != null) {
                mFooter.setVisibility(View.GONE);
            }

            mAddTab.setVisibility(View.INVISIBLE);

            mMenuButton.setVisibility(View.GONE);
        } else {
            if (mFooter != null)
                mFooter.setVisibility(View.VISIBLE);

            mAddTab.setVisibility(View.VISIBLE);
            mAddTab.setImageLevel(index);


            if (!HardwareUtils.hasMenuButton()) {
                mMenuButton.setVisibility(View.VISIBLE);
                mMenuButton.setEnabled(true);
                mPopupMenu.setAnchor(mMenuButton);
            } else {
                mPopupMenu.setAnchor(mAddTab);
            }
        }

        if (isSideBar()) {
            if (showAnimation)
                dispatchLayoutChange(getWidth(), getHeight());
        } else {
            int actionBarHeight = mContext.getResources().getDimensionPixelSize(R.dimen.browser_toolbar_height);
            int height = actionBarHeight + getTabContainerHeight(mTabsContainer);
            dispatchLayoutChange(getWidth(), height);
        }
        mHeaderVisible = true;
    }

    public void hide() {
        mHeaderVisible = false;

        if (mVisible) {
            mVisible = false;
            mPopupMenu.dismiss();
            dispatchLayoutChange(0, 0);
        }
    }

    public void refresh() {
        removeAllViews();

        inflateLayout(mContext);
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
        if (Versions.preHC) {
            return;
        }

        if (mIsSideBar) {
            final int tabsPanelWidth = getWidth();
            if (mVisible) {
                ViewHelper.setTranslationX(mHeader, -tabsPanelWidth);
                ViewHelper.setTranslationX(mTabsContainer, -tabsPanelWidth);

                // The footer view is only present on the sidebar, v11+.
                ViewHelper.setTranslationX(mFooter, -tabsPanelWidth);
            }
            final int translationX = (mVisible ? 0 : -tabsPanelWidth);
            animator.attach(mTabsContainer, PropertyAnimator.Property.TRANSLATION_X, translationX);
            animator.attach(mHeader, PropertyAnimator.Property.TRANSLATION_X, translationX);
            animator.attach(mFooter, PropertyAnimator.Property.TRANSLATION_X, translationX);

        } else if (!mHeaderVisible) {
            final Resources resources = getContext().getResources();
            final int toolbarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height);
            final int translationY = (mVisible ? 0 : -toolbarHeight);
            if (mVisible) {
                ViewHelper.setTranslationY(mHeader, -toolbarHeight);
                ViewHelper.setTranslationY(mTabsContainer, -toolbarHeight);
                ViewHelper.setAlpha(mTabsContainer, 0.0f);
            }
            animator.attach(mTabsContainer, PropertyAnimator.Property.ALPHA, mVisible ? 1.0f : 0.0f);
            animator.attach(mTabsContainer, PropertyAnimator.Property.TRANSLATION_Y, translationY);
            animator.attach(mHeader, PropertyAnimator.Property.TRANSLATION_Y, translationY);
        }

        mHeader.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        mTabsContainer.setLayerType(View.LAYER_TYPE_HARDWARE, null);
    }

    public void finishTabsAnimation() {
        if (Versions.preHC) {
            return;
        }

        mHeader.setLayerType(View.LAYER_TYPE_NONE, null);
        mTabsContainer.setLayerType(View.LAYER_TYPE_NONE, null);

        // If the tabs panel is now hidden, call hide() on current panel and unset it as the current panel
        // to avoid hide() being called again when the layout is opened next.
        if (!mVisible && mPanel != null) {
            mPanel.hide();
            mPanel = null;
        }
    }

    public void setTabsLayoutChangeListener(TabsLayoutChangeListener listener) {
        mLayoutChangeListener = listener;
    }

    private void dispatchLayoutChange(int width, int height) {
        if (mLayoutChangeListener != null)
            mLayoutChangeListener.onTabsLayoutChange(width, height);
    }

    /**
     * Fetch the Drawable icon corresponding to the given panel.
     * @param panel to fetch icon for.
     * @return Drawable instance, or null if no icon is being displayed, or the icon does not exist.
     */
    public Drawable getIconDrawable(Panel panel) {
        return mTabWidget.getIconDrawable(panel.ordinal());
    }

    public void setIconDrawable(Panel panel, int resource) {
        mTabWidget.setIconDrawable(panel.ordinal(), resource);
    }
}
