/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.lwt.LightweightTheme;
import org.mozilla.gecko.lwt.LightweightThemeDrawable;
import org.mozilla.gecko.restrictions.Restriction;
import org.mozilla.gecko.util.ColorUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.widget.GeckoPopupMenu;
import org.mozilla.gecko.widget.IconTabWidget;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import org.mozilla.gecko.widget.ThemedImageButton;

public class TabsPanel extends LinearLayout
                       implements GeckoPopupMenu.OnMenuItemClickListener,
                                  LightweightTheme.OnChangeListener,
                                  IconTabWidget.OnTabChangedListener {
    private static final String LOGTAG = "Gecko" + TabsPanel.class.getSimpleName();

    public enum Panel {
        NORMAL_TABS,
        PRIVATE_TABS,
    }

    public interface PanelView {
        void setTabsPanel(TabsPanel panel);
        void show();
        void hide();
        boolean shouldExpand();
    }

    public interface CloseAllPanelView extends PanelView {
        void closeAll();
    }

    public interface TabsLayout extends CloseAllPanelView {
        void setEmptyView(View view);
    }

    public static View createTabsLayout(final Context context, final AttributeSet attrs) {
        if (HardwareUtils.isTablet()) {
            return new TabsGridLayout(context, attrs);
        } else {
            return new TabsListLayout(context, attrs);
        }
    }

    public interface TabsLayoutChangeListener {
        void onTabsLayoutChange(int width, int height);
    }

    private final Context mContext;
    private final GeckoApp mActivity;
    private final LightweightTheme mTheme;
    private RelativeLayout mHeader;
    private FrameLayout mTabsContainer;
    private PanelView mPanel;
    private PanelView mPanelNormal;
    private PanelView mPanelPrivate;
    private TabsLayoutChangeListener mLayoutChangeListener;

    private IconTabWidget mTabWidget;
    private static ImageButton mMenuButton;
    private static ImageButton mAddTab;
    private ImageButton mNavBackButton;

    private Panel mCurrentPanel;
    private boolean mVisible;
    private boolean mHeaderVisible;

    private final GeckoPopupMenu mPopupMenu;

    public TabsPanel(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mActivity = (GeckoApp) context;
        mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();

        mCurrentPanel = Panel.NORMAL_TABS;

        mPopupMenu = new GeckoPopupMenu(context);
        mPopupMenu.inflate(R.menu.tabs_menu);
        mPopupMenu.setOnMenuItemClickListener(this);

        inflateLayout(context);
        initialize();
    }

    private void inflateLayout(Context context) {
        LayoutInflater.from(context).inflate(R.layout.tabs_panel_default, this);
    }

    private void initialize() {
        mHeader = (RelativeLayout) findViewById(R.id.tabs_panel_header);
        mTabsContainer = (FrameLayout) findViewById(R.id.tabs_container);

        mPanelNormal = (PanelView) findViewById(R.id.normal_tabs);
        mPanelNormal.setTabsPanel(this);

        mPanelPrivate = (PanelView) findViewById(R.id.private_tabs_panel);
        mPanelPrivate.setTabsPanel(this);

        mAddTab = (ImageButton) findViewById(R.id.add_tab);
        mAddTab.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View v) {
                TabsPanel.this.addTab();
            }
        });

        mTabWidget = (IconTabWidget) findViewById(R.id.tab_widget);

        mTabWidget.addTab(R.drawable.tabs_normal, R.string.tabs_normal);
        final ThemedImageButton privateTabsPanel =
                (ThemedImageButton) mTabWidget.addTab(R.drawable.tabs_private, R.string.tabs_private);
        privateTabsPanel.setPrivateMode(true);

        if (!RestrictedProfiles.isAllowed(mContext, Restriction.DISALLOW_PRIVATE_BROWSING)) {
            mTabWidget.setVisibility(View.GONE);
        }

        mTabWidget.setTabSelectionListener(this);

        mMenuButton = (ImageButton) findViewById(R.id.menu);
        mMenuButton.setOnClickListener(new Button.OnClickListener() {
            @Override
            public void onClick(View view) {
                showMenu();
            }
        });

        if (HardwareUtils.isTablet()) {
            ViewStub backButtonStub = (ViewStub) findViewById(R.id.nav_back_stub);
            mNavBackButton = (ImageButton) backButtonStub.inflate( );
            mNavBackButton.setOnClickListener(new Button.OnClickListener() {
                @Override
                public void onClick(View view) {
                    mActivity.onBackPressed();
                }
            });
        }
    }

    public void showMenu() {
        final Menu menu = mPopupMenu.getMenu();

        // Each panel has a "+" shortcut button, so don't show it for that panel.
        menu.findItem(R.id.new_tab).setVisible(mCurrentPanel != Panel.NORMAL_TABS);
        menu.findItem(R.id.new_private_tab).setVisible(mCurrentPanel != Panel.PRIVATE_TABS
                && RestrictedProfiles.isAllowed(mContext, Restriction.DISALLOW_PRIVATE_BROWSING));

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
        } else {
            show(Panel.PRIVATE_TABS);
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

    private static int getTabContainerHeight(View tabsContainer) {
        Resources resources = tabsContainer.getContext().getResources();

        int screenHeight = resources.getDisplayMetrics().heightPixels;

        int actionBarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height);

        return screenHeight - actionBarHeight;
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
        final int background = ColorUtils.getColor(getContext(), R.color.text_and_tabs_tray_grey);
        final LightweightThemeDrawable drawable = mTheme.getColorDrawable(this, background, true);
        if (drawable == null)
            return;

        drawable.setAlpha(34, 0);
        setBackgroundDrawable(drawable);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundColor(ColorUtils.getColor(getContext(), R.color.text_and_tabs_tray_grey));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        onLightweightThemeChanged();
    }

    // Tabs Panel Toolbar contains the Buttons
    static class TabsPanelToolbar extends LinearLayout
                                  implements LightweightTheme.OnChangeListener {
        private final LightweightTheme mTheme;

        public TabsPanelToolbar(Context context, AttributeSet attrs) {
            super(context, attrs);
            mTheme = ((GeckoApplication) context.getApplicationContext()).getLightweightTheme();
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
            final int background = ColorUtils.getColor(getContext(), R.color.text_and_tabs_tray_grey);
            final LightweightThemeDrawable drawable = mTheme.getColorDrawable(this, background);
            if (drawable == null)
                return;

            drawable.setAlpha(34, 34);
            setBackgroundDrawable(drawable);
        }

        @Override
        public void onLightweightThemeReset() {
            setBackgroundColor(ColorUtils.getColor(getContext(), R.color.text_and_tabs_tray_grey));
        }

        @Override
        protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
            super.onLayout(changed, left, top, right, bottom);
            onLightweightThemeChanged();
        }
    }

    public void show(Panel panelToShow) {
        prepareToShow(panelToShow);
        int height = getVerticalPanelHeight();
        dispatchLayoutChange(getWidth(), height);
        mHeaderVisible = true;
    }

    public void prepareToDrag() {
        Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null && selectedTab.isPrivate()) {
            prepareToShow(TabsPanel.Panel.PRIVATE_TABS);
        } else {
            prepareToShow(TabsPanel.Panel.NORMAL_TABS);
        }
        mHeaderVisible = true;
    }

    public void prepareToShow(Panel panelToShow) {
        if (!isShown()) {
            setVisibility(View.VISIBLE);
        }

        if (mPanel != null) {
            // Hide the old panel.
            mPanel.hide();
        }

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

            default:
                throw new IllegalArgumentException("Unknown panel type " + panelToShow);
        }
        mPanel.show();

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

    public void hideImmediately() {
        mVisible = false;
        setVisibility(View.INVISIBLE);
    }

    public int getVerticalPanelHeight() {
        final int actionBarHeight = mContext.getResources().getDimensionPixelSize(R.dimen.browser_toolbar_height);
        final int height = actionBarHeight + getTabContainerHeight(mTabsContainer);
        return height;
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

    public Panel getCurrentPanel() {
        return mCurrentPanel;
    }

    public void setHWLayerEnabled(boolean enabled) {
        if (Versions.preHC) {
            return;
        }
        if (enabled) {
            mHeader.setLayerType(View.LAYER_TYPE_HARDWARE, null);
            mTabsContainer.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        } else {
            mHeader.setLayerType(View.LAYER_TYPE_NONE, null);
            mTabsContainer.setLayerType(View.LAYER_TYPE_NONE, null);
        }
    }

    public void prepareTabsAnimation(PropertyAnimator animator) {
        // Not worth doing this on pre-Honeycomb without proper
        // hardware accelerated animations.
        if (Versions.preHC) {
            return;
        }

        if (!mHeaderVisible) {
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

        setHWLayerEnabled(true);
    }

    public void translateInRange(float progress) {
        final Resources resources = getContext().getResources();
        final int toolbarHeight = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height);
        final int translationY =  (int) - ((1 - progress) * toolbarHeight);
        ViewHelper.setTranslationY(mHeader, translationY);
        ViewHelper.setTranslationY(mTabsContainer, translationY);
        mTabsContainer.setAlpha(progress);
    }

    public void finishTabsAnimation() {
        if (Versions.preHC) {
            return;
        }

        setHWLayerEnabled(false);

        // If the tray is now hidden, call hide() on current panel and unset it as the current panel
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
}
