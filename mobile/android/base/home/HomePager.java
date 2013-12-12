/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.home.HomeAdapter.OnAddPageListener;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.view.ViewPager;
import android.view.ViewGroup.LayoutParams;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.View;

import java.util.EnumSet;

public class HomePager extends ViewPager {

    private final Context mContext;
    private volatile boolean mLoaded;
    private Decor mDecor;

    private final OnAddPageListener mAddPageListener;

    // List of pages in order.
    @RobocopTarget
    public enum Page {
        HISTORY,
        TOP_SITES,
        BOOKMARKS,
        READING_LIST
    }

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

    public HomePager(Context context) {
        this(context, null);
    }

    public HomePager(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;

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
        }

        super.addView(child, index, params);
    }

    public void redisplay(FragmentManager fm) {
        final HomeAdapter adapter = (HomeAdapter) getAdapter();
        final Page currentPage = adapter.getPageAtPosition(getCurrentItem());

        show(fm, currentPage, null);
    }

    /**
     * Loads and initializes the pager.
     *
     * @param fm FragmentManager for the adapter
     */
    public void show(FragmentManager fm, Page page, PropertyAnimator animator) {
        mLoaded = true;

        if (mDecor != null) {
            mDecor.removeAllPagerViews();
        }

        final HomeAdapter adapter = new HomeAdapter(mContext, fm);
        adapter.setOnAddPageListener(mAddPageListener);

        // Only animate on post-HC devices, when a non-null animator is given
        final boolean shouldAnimate = (animator != null && Build.VERSION.SDK_INT >= 11);

        adapter.addPage(Page.TOP_SITES, TopSitesPage.class, new Bundle(),
                getContext().getString(R.string.home_top_sites_title));
        adapter.addPage(Page.BOOKMARKS, BookmarksPage.class, new Bundle(),
                getContext().getString(R.string.bookmarks_title));

        // We disable reader mode support on low memory devices. Hence the
        // reading list page should not show up on such devices.
        if (!HardwareUtils.isLowMemoryPlatform()) {
            adapter.addPage(Page.READING_LIST, ReadingListPage.class, new Bundle(),
                    getContext().getString(R.string.reading_list_title));
        }

        // On phones, the history page is the first one. On tablets, the
        // history page is the last.
        adapter.addPage(HardwareUtils.isTablet() ? -1 : 0,
                Page.HISTORY, HistoryPage.class, new Bundle(),
                getContext().getString(R.string.home_history_title));

        adapter.setCanLoadHint(!shouldAnimate);

        setAdapter(adapter);

        setCurrentItem(adapter.getItemPosition(page), false);
        setVisibility(VISIBLE);

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
}
