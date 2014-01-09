/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.home.HomeAdapter.OnAddPageListener;
import org.mozilla.gecko.home.HomeConfig.PageEntry;
import org.mozilla.gecko.home.HomeConfig.PageType;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.view.ViewPager;
import android.view.ViewGroup.LayoutParams;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.View;

import java.util.EnumSet;
import java.util.List;

public class HomePager extends ViewPager {

    private static final int LOADER_ID_CONFIG = 0;

    private final Context mContext;
    private volatile boolean mLoaded;
    private Decor mDecor;
    private View mTabStrip;

    private final OnAddPageListener mAddPageListener;

    private final HomeConfig mConfig;
    private ConfigLoaderCallbacks mConfigLoaderCallbacks;

    private String mInitialPageId;

    // Whether or not we need to restart the loader when we show the HomePager.
    private boolean mRestartLoader;

    // This is mostly used by UI tests to easily fetch
    // specific list views at runtime.
    static final String LIST_TAG_HISTORY = "history";
    static final String LIST_TAG_BOOKMARKS = "bookmarks";
    static final String LIST_TAG_READING_LIST = "reading_list";
    static final String LIST_TAG_TOP_SITES = "top_sites";
    static final String LIST_TAG_MOST_RECENT = "most_recent";
    static final String LIST_TAG_LAST_TABS = "last_tabs";
    static final String LIST_TAG_BROWSER_SEARCH = "browser_search";

    public interface OnUrlOpenListener {
        public enum Flags {
            ALLOW_SWITCH_TO_TAB
        }

        public void onUrlOpen(String url, EnumSet<Flags> flags);
    }

    public interface OnNewTabsListener {
        public void onNewTabs(String[] urls);
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

    static final String CAN_LOAD_ARG = "canLoad";
    static final String PAGE_ENTRY_ARG = "pageEntry";

    public HomePager(Context context) {
        this(context, null);
    }

    public HomePager(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

        mConfig = HomeConfig.getDefault(mContext);
        mConfigLoaderCallbacks = new ConfigLoaderCallbacks();

        mAddPageListener = new OnAddPageListener() {
            @Override
            public void onAddPage(String title) {
                if (mDecor != null) {
                    mDecor.onAddPagerView(title);
                }
            }
        };

        // This is to keep all 4 pages in memory after they are
        // selected in the pager.
        setOffscreenPageLimit(3);

        //  We can call HomePager.requestFocus to steal focus from the URL bar and drop the soft
        //  keyboard. However, if there are no focusable views (e.g. an empty reading list), the
        //  URL bar will be refocused. Therefore, we make the HomePager container focusable to
        //  ensure there is always a focusable view. This would ordinarily be done via an XML
        //  attribute, but it is not working properly.
        setFocusableInTouchMode(true);
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

            setOnPageChangeListener(new ViewPager.OnPageChangeListener() {
                @Override
                public void onPageSelected(int position) {
                    mDecor.onPageSelected(position);
                }

                @Override
                public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
                    mDecor.onPageScrolled(position, positionOffset, positionOffsetPixels);
                }

                @Override
                public void onPageScrollStateChanged(int state) { }
            });
        } else if (child instanceof HomePagerTabStrip) {
            mTabStrip = child;
        }

        super.addView(child, index, params);
    }

    /**
     * Invalidates the current configuration, redisplaying the HomePager if necessary.
     */
    public void invalidate(LoaderManager lm, FragmentManager fm) {
        // We need to restart the loader to load the new strings.
        mRestartLoader = true;

        // If the HomePager is currently visible, redisplay it with the new strings.
        if (isVisible()) {
            redisplay(lm, fm);
        }
    }

    private void redisplay(LoaderManager lm, FragmentManager fm) {
        final HomeAdapter adapter = (HomeAdapter) getAdapter();

        // If mInitialPageId is non-null, this means the HomePager hasn't
        // finished loading its config yet. Simply re-show() with the
        // current target page.
        final String currentPageId;
        if (mInitialPageId != null) {
            currentPageId = mInitialPageId;
        } else {
            currentPageId = adapter.getPageIdAtPosition(getCurrentItem());
        }

        show(lm, fm, currentPageId, null);
    }

    /**
     * Loads and initializes the pager.
     *
     * @param fm FragmentManager for the adapter
     */
    public void show(LoaderManager lm, FragmentManager fm, String pageId, PropertyAnimator animator) {
        mLoaded = true;
        mInitialPageId = pageId;

        // Only animate on post-HC devices, when a non-null animator is given
        final boolean shouldAnimate = (animator != null && Build.VERSION.SDK_INT >= 11);

        final HomeAdapter adapter = new HomeAdapter(mContext, fm);
        adapter.setOnAddPageListener(mAddPageListener);
        adapter.setCanLoadHint(!shouldAnimate);
        setAdapter(adapter);

        setVisibility(VISIBLE);

        // Don't show the tabs strip until we have the
        // list of pages in place.
        mTabStrip.setVisibility(View.INVISIBLE);

        // Load list of pages from configuration. Restart the loader if necessary.
        if (mRestartLoader) {
            lm.restartLoader(LOADER_ID_CONFIG, null, mConfigLoaderCallbacks);
            mRestartLoader = false;
        } else {
            lm.initLoader(LOADER_ID_CONFIG, null, mConfigLoaderCallbacks);
        }

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
    }

    /**
     * Hides the pager and removes all child fragments.
     */
    public void hide() {
        mLoaded = false;
        setVisibility(GONE);
        setAdapter(null);
    }

    /**
     * Determines whether the pager is visible.
     *
     * Unlike getVisibility(), this method does not need to be called on the UI
     * thread.
     *
     * @return Whether the pager and its fragments are being displayed
     */
    public boolean isVisible() {
        return mLoaded;
    }

    @Override
    public void setCurrentItem(int item, boolean smoothScroll) {
        super.setCurrentItem(item, smoothScroll);

        if (mDecor != null) {
            mDecor.onPageSelected(item);
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

    private void updateUiFromPageEntries(List<PageEntry> pageEntries) {
        // We only care about the adapter if HomePager is currently
        // loaded, which means it's visible in the activity.
        if (!mLoaded) {
            return;
        }

        if (mDecor != null) {
            mDecor.removeAllPagerViews();
        }

        final HomeAdapter adapter = (HomeAdapter) getAdapter();

        // Disable loading until the final current item is defined
        // after loading the page entries. This is to stop any temporary
        // active item from loading.
        boolean originalCanLoadHint = adapter.getCanLoadHint();
        adapter.setCanLoadHint(false);

        // Update the adapter with the new page entries
        adapter.update(pageEntries);

        final int count = (pageEntries != null ? pageEntries.size() : 0);
        mTabStrip.setVisibility(count > 0 ? View.VISIBLE : View.INVISIBLE);

        // Use the default page as defined in the HomePager's configuration
        // if the initial page wasn't explicitly set by the show() caller.
        if (mInitialPageId != null) {
            // XXX: Handle the case where the desired page isn't currently in the adapter (bug 949178)
            setCurrentItem(adapter.getItemPosition(mInitialPageId), false);
            mInitialPageId = null;
        } else {
            for (int i = 0; i < count; i++) {
                final PageEntry pageEntry = pageEntries.get(i);
                if (pageEntry.isDefault()) {
                    setCurrentItem(i, false);
                    break;
                }
            }
        }

        // Restore canLoadHint now that we have the final
        // state in HomePager.
        adapter.setCanLoadHint(originalCanLoadHint);
    }

    private class ConfigLoaderCallbacks implements LoaderCallbacks<List<PageEntry>> {
        @Override
        public Loader<List<PageEntry>> onCreateLoader(int id, Bundle args) {
            return new HomeConfigLoader(mContext, mConfig);
        }

        @Override
        public void onLoadFinished(Loader<List<PageEntry>> loader, List<PageEntry> pageEntries) {
            updateUiFromPageEntries(pageEntries);
        }

        @Override
        public void onLoaderReset(Loader<List<PageEntry>> loader) {
            updateUiFromPageEntries(null);
        }
    }
}
