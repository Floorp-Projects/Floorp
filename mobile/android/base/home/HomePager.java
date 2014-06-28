/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.home.HomeAdapter.OnAddPanelListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

public class HomePager extends ViewPager {
    private static final int LOADER_ID_CONFIG = 0;

    private final Context mContext;
    private volatile boolean mVisible;
    private Decor mDecor;
    private View mTabStrip;
    private HomeBanner mHomeBanner;
    private int mDefaultPageIndex = -1;

    private final OnAddPanelListener mAddPanelListener;

    private final HomeConfig mConfig;
    private ConfigLoaderCallbacks mConfigLoaderCallbacks;

    private String mInitialPanelId;

    // Cached original ViewPager background.
    private final Drawable mOriginalBackground;

    // Telemetry session for current panel.
    private TelemetryContract.Session mCurrentPanelSession;
    private String mCurrentPanelSessionSuffix;

    // Current load state of HomePager.
    private LoadState mLoadState;

    // Listens for when the current panel changes.
    private OnPanelChangeListener mPanelChangedListener;

    // This is mostly used by UI tests to easily fetch
    // specific list views at runtime.
    public static final String LIST_TAG_HISTORY = "history";
    public static final String LIST_TAG_BOOKMARKS = "bookmarks";
    public static final String LIST_TAG_READING_LIST = "reading_list";
    public static final String LIST_TAG_TOP_SITES = "top_sites";
    public static final String LIST_TAG_RECENT_TABS = "recent_tabs";
    public static final String LIST_TAG_BROWSER_SEARCH = "browser_search";

    public interface OnUrlOpenListener {
        public enum Flags {
            ALLOW_SWITCH_TO_TAB,
            OPEN_WITH_INTENT
        }

        public void onUrlOpen(String url, EnumSet<Flags> flags);
    }

    public interface OnNewTabsListener {
        public void onNewTabs(List<String> urls);
    }

    /**
     * Interface for listening into ViewPager panel changes
     */
    public interface OnPanelChangeListener {
        /**
         * Called when a new panel is selected.
         *
         * @param panelId of the newly selected panel
         */
        public void onPanelSelected(String panelId);
    }

    interface OnTitleClickListener {
        public void onTitleClicked(int index);
    }

    /**
     * Special type of child views that could be added as pager decorations by default.
     */
    interface Decor {
        public void onAddPagerView(String title);
        public void removeAllPagerViews();
        public void onPageSelected(int position);
        public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels);
        public void setOnTitleClickListener(OnTitleClickListener onTitleClickListener);
    }

    /**
     * State of HomePager with respect to loading its configuration.
     */
    private enum LoadState {
        UNLOADED,
        LOADING,
        LOADED
    }

    static final String CAN_LOAD_ARG = "canLoad";
    static final String PANEL_CONFIG_ARG = "panelConfig";

    public HomePager(Context context) {
        this(context, null);
    }

    public HomePager(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mConfig = HomeConfig.getDefault(mContext);
        mConfigLoaderCallbacks = new ConfigLoaderCallbacks();

        mAddPanelListener = new OnAddPanelListener() {
            @Override
            public void onAddPanel(String title) {
                if (mDecor != null) {
                    mDecor.onAddPagerView(title);
                }
            }
        };

        // This is to keep all 4 panels in memory after they are
        // selected in the pager.
        setOffscreenPageLimit(3);

        //  We can call HomePager.requestFocus to steal focus from the URL bar and drop the soft
        //  keyboard. However, if there are no focusable views (e.g. an empty reading list), the
        //  URL bar will be refocused. Therefore, we make the HomePager container focusable to
        //  ensure there is always a focusable view. This would ordinarily be done via an XML
        //  attribute, but it is not working properly.
        setFocusableInTouchMode(true);

        mOriginalBackground = getBackground();
        setOnPageChangeListener(new PageChangeListener());

        mLoadState = LoadState.UNLOADED;
    }

    @Override
    public void addView(View child, int index, ViewGroup.LayoutParams params) {
        if (child instanceof Decor) {
            ((ViewPager.LayoutParams) params).isDecor = true;
            mDecor = (Decor) child;
            mTabStrip = child;

            mDecor.setOnTitleClickListener(new OnTitleClickListener() {
                @Override
                public void onTitleClicked(int index) {
                    setCurrentItem(index, true);
                }
            });
        } else if (child instanceof HomePagerTabStrip) {
            mTabStrip = child;
        }

        super.addView(child, index, params);
    }

    /**
     * Loads and initializes the pager.
     *
     * @param fm FragmentManager for the adapter
     */
    public void load(LoaderManager lm, FragmentManager fm, String panelId, PropertyAnimator animator) {
        mLoadState = LoadState.LOADING;

        mVisible = true;
        mInitialPanelId = panelId;

        // Update the home banner message each time the HomePager is loaded.
        if (mHomeBanner != null) {
            mHomeBanner.update();
        }

        // Only animate on post-HC devices, when a non-null animator is given
        final boolean shouldAnimate = (animator != null && Build.VERSION.SDK_INT >= 11);

        final HomeAdapter adapter = new HomeAdapter(mContext, fm);
        adapter.setOnAddPanelListener(mAddPanelListener);
        adapter.setCanLoadHint(!shouldAnimate);
        setAdapter(adapter);

        // Don't show the tabs strip until we have the
        // list of panels in place.
        mTabStrip.setVisibility(View.INVISIBLE);

        // Load list of panels from configuration
        lm.initLoader(LOADER_ID_CONFIG, null, mConfigLoaderCallbacks);

        if (shouldAnimate) {
            animator.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
                @Override
                public void onPropertyAnimationStart() {
                    setLayerType(View.LAYER_TYPE_HARDWARE, null);
                }

                @Override
                public void onPropertyAnimationEnd() {
                    setLayerType(View.LAYER_TYPE_NONE, null);
                    adapter.setCanLoadHint(true);
                }
            });

            ViewHelper.setAlpha(this, 0.0f);

            animator.attach(this,
                            PropertyAnimator.Property.ALPHA,
                            1.0f);
        }
        Telemetry.startUISession(TelemetryContract.Session.HOME);
    }

    /**
     * Removes all child fragments to free memory.
     */
    public void unload() {
        mVisible = false;
        setAdapter(null);
        mLoadState = LoadState.UNLOADED;

        // Stop UI Telemetry sessions.
        stopCurrentPanelTelemetrySession();
        Telemetry.stopUISession(TelemetryContract.Session.HOME);
    }

    /**
     * Determines whether the pager is visible.
     *
     * Unlike getVisibility(), this method does not need to be called on the UI
     * thread.
     *
     * @return Whether the pager and its fragments are loaded
     */
    public boolean isVisible() {
        return mVisible;
    }

    @Override
    public void setCurrentItem(int item, boolean smoothScroll) {
        super.setCurrentItem(item, smoothScroll);

        if (mDecor != null) {
            mDecor.onPageSelected(item);
        }

        if (mHomeBanner != null) {
            mHomeBanner.setActive(item == mDefaultPageIndex);
        }
    }

    /**
     * Shows a home panel. If the given panelId is null,
     * the default panel will be shown. No action will be taken if:
     *  * HomePager has not loaded yet
     *  * Panel with the given panelId cannot be found
     *
     * @param panelId of the home panel to be shown.
     */
    public void showPanel(String panelId) {
        if (!mVisible) {
            return;
        }

        switch (mLoadState) {
            case LOADING:
                mInitialPanelId = panelId;
                break;

            case LOADED:
                int position = mDefaultPageIndex;
                if (panelId != null) {
                    position = ((HomeAdapter) getAdapter()).getItemPosition(panelId);
                }

                if (position > -1) {
                    setCurrentItem(position);
                }
                break;

            default:
                // Do nothing.
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            // Drop the soft keyboard by stealing focus from the URL bar.
            requestFocus();
        }

        return super.onInterceptTouchEvent(event);
    }

    public void setBanner(HomeBanner banner) {
        mHomeBanner = banner;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (mHomeBanner != null) {
            mHomeBanner.handleHomeTouch(event);
        }

        return super.dispatchTouchEvent(event);
    }

    public void onToolbarFocusChange(boolean hasFocus) {
        if (mHomeBanner == null) {
            return;
        }

        // We should only make the banner active if the toolbar is not focused and we are on the default page
        final boolean active = !hasFocus && getCurrentItem() == mDefaultPageIndex;
        mHomeBanner.setActive(active);
    }

    private void updateUiFromConfigState(HomeConfig.State configState) {
        // We only care about the adapter if HomePager is currently
        // loaded, which means it's visible in the activity.
        if (!mVisible) {
            return;
        }

        if (mDecor != null) {
            mDecor.removeAllPagerViews();
        }

        final HomeAdapter adapter = (HomeAdapter) getAdapter();

        // Disable any fragment loading until we have the initial
        // panel selection done. Store previous value to restore
        // it if necessary once the UI is fully updated.
        final boolean canLoadHint = adapter.getCanLoadHint();
        adapter.setCanLoadHint(false);

        // Destroy any existing panels currently loaded
        // in the pager.
        setAdapter(null);

        // Only keep enabled panels.
        final List<PanelConfig> enabledPanels = new ArrayList<PanelConfig>();

        for (PanelConfig panelConfig : configState) {
            if (!panelConfig.isDisabled()) {
                enabledPanels.add(panelConfig);
            }
        }

        // Update the adapter with the new panel configs
        adapter.update(enabledPanels);

        final int count = enabledPanels.size();
        if (count == 0) {
            // Set firefox watermark as background.
            setBackgroundResource(R.drawable.home_pager_empty_state);
            // Hide the tab strip as there are no panels.
            mTabStrip.setVisibility(View.INVISIBLE);
        } else {
            mTabStrip.setVisibility(View.VISIBLE);
            // Restore original background.
            setBackgroundDrawable(mOriginalBackground);
        }

        // Re-install the adapter with the final state
        // in the pager.
        setAdapter(adapter);

        if (count == 0) {
            mDefaultPageIndex = -1;

            // Hide the banner if there are no enabled panels.
            if (mHomeBanner != null) {
                mHomeBanner.setActive(false);
            }
        } else {
            for (int i = 0; i < count; i++) {
                if (enabledPanels.get(i).isDefault()) {
                    mDefaultPageIndex = i;
                    break;
                }
            }

            // Use the default panel if the initial panel wasn't explicitly set by the
            // load() caller, or if the initial panel is not found in the adapter.
            final int itemPosition = (mInitialPanelId == null) ? -1 : adapter.getItemPosition(mInitialPanelId);
            if (itemPosition > -1) {
                setCurrentItem(itemPosition, false);
                mInitialPanelId = null;
            } else {
                setCurrentItem(mDefaultPageIndex, false);
            }
        }

        // If the load hint was originally true, this means the pager
        // is not animating and it's fine to restore the load hint back.
        if (canLoadHint) {
            // The selection is updated asynchronously so we need to post to
            // UI thread to give the pager time to commit the new page selection
            // internally and load the right initial panel.
            ThreadUtils.getUiHandler().post(new Runnable() {
                @Override
                public void run() {
                    adapter.setCanLoadHint(true);
                }
            });
        }
    }

    public void setOnPanelChangeListener(OnPanelChangeListener listener) {
       mPanelChangedListener = listener;
    }

    /**
     * Notify listeners of newly selected panel.
     *
     * @param position of the newly selected panel
     */
    private void notifyPanelSelected(int position) {
        if (mDecor != null) {
            mDecor.onPageSelected(position);
        }

        if (mPanelChangedListener != null) {
            final String panelId = ((HomeAdapter) getAdapter()).getPanelIdAtPosition(position);
            mPanelChangedListener.onPanelSelected(panelId);
        }
    }

    private class ConfigLoaderCallbacks implements LoaderCallbacks<HomeConfig.State> {
        @Override
        public Loader<HomeConfig.State> onCreateLoader(int id, Bundle args) {
            return new HomeConfigLoader(mContext, mConfig);
        }

        @Override
        public void onLoadFinished(Loader<HomeConfig.State> loader, HomeConfig.State configState) {
            mLoadState = LoadState.LOADED;
            updateUiFromConfigState(configState);
        }

        @Override
        public void onLoaderReset(Loader<HomeConfig.State> loader) {
            mLoadState = LoadState.UNLOADED;
        }
    }

    private class PageChangeListener implements ViewPager.OnPageChangeListener {
        @Override
        public void onPageSelected(int position) {
            notifyPanelSelected(position);

            if (mHomeBanner != null) {
                mHomeBanner.setActive(position == mDefaultPageIndex);
            }

            // Start a UI telemetry session for the newly selected panel.
            final String newPanelId = ((HomeAdapter) getAdapter()).getPanelIdAtPosition(position);
            startNewPanelTelemetrySession(newPanelId);
        }

        @Override
        public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
            if (mDecor != null) {
                mDecor.onPageScrolled(position, positionOffset, positionOffsetPixels);
            }

            if (mHomeBanner != null) {
                mHomeBanner.setScrollingPages(positionOffsetPixels != 0);
            }
        }

        @Override
        public void onPageScrollStateChanged(int state) { }
    }

    /**
     * Start UI telemetry session for the a panel.
     * If there is currently a session open for a panel,
     * it will be stopped before a new one is started.
     *
     * @param panelId of panel to start a session for
     */
    private void startNewPanelTelemetrySession(String panelId) {
        // Stop the current panel's session if we have one.
        stopCurrentPanelTelemetrySession();

        mCurrentPanelSession = TelemetryContract.Session.HOME_PANEL;
        mCurrentPanelSessionSuffix = panelId;
        Telemetry.startUISession(mCurrentPanelSession, mCurrentPanelSessionSuffix);
    }

    /**
     * Stop the current panel telemetry session if one exists.
     */
    private void stopCurrentPanelTelemetrySession() {
        if (mCurrentPanelSession != null) {
            Telemetry.stopUISession(mCurrentPanelSession, mCurrentPanelSessionSuffix);
            mCurrentPanelSession = null;
            mCurrentPanelSessionSuffix = null;
        }
    }
}
