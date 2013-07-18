/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.AboutHome;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentStatePagerAdapter;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.EnumMap;
import java.util.EnumSet;

public class HomePager extends ViewPager {
    // Subpage fragment tag
    public static final String SUBPAGE_TAG = "home_pager_subpage";

    private final Context mContext;
    private volatile boolean mLoaded;

    // List of pages in order.
    private enum Page {
        VISITED,
        BOOKMARKS,
        READING_LIST
    }

    private EnumMap<Page, Fragment> mPages = new EnumMap<Page, Fragment>(Page.class);

    public interface OnUrlOpenListener {
        public void onUrlOpen(String url);
    }

    public interface OnNewTabsListener {
        public void onNewTabs(String[] urls);
    }

    public HomePager(Context context) {
        super(context);
        mContext = context;
    }

    public HomePager(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    /**
     * Loads and initializes the pager.
     *
     * @param fm FragmentManager for the adapter
     */
    public void show(FragmentManager fm) {
        mLoaded = true;
        TabsAdapter adapter = new TabsAdapter(fm);

        // Add the pages to the adapter in order.
        adapter.addTab(Page.VISITED, VisitedPage.class, null, getContext().getString(R.string.visited_title));
        adapter.addTab(Page.BOOKMARKS, BookmarksPage.class, null, getContext().getString(R.string.bookmarks_title));
        adapter.addTab(Page.READING_LIST, ReadingListPage.class, null, getContext().getString(R.string.reading_list));

        setAdapter(adapter);

        // Set the bookmarks page as the landing page.
        setCurrentItem(1, false);
        setVisibility(VISIBLE);
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

    /**
     * @see AboutHome#setLastTabsVisibility(boolean)
     */
    public void setAboutHomeLastTabsVisibility(boolean visible) {
        // FIXME: Page handling last tabs should update it.
    }

    class TabsAdapter extends FragmentStatePagerAdapter {
        private final ArrayList<TabInfo> mTabs = new ArrayList<TabInfo>();

        final class TabInfo {
            private final Page page;
            private final Class<?> clss;
            private final Bundle args;
            private final String title;

            TabInfo(Page page, Class<?> clss, Bundle args, String title) {
                this.page = page;
                this.clss = clss;
                this.args = args;
                this.title = title;
            }
        }

        public TabsAdapter(FragmentManager fm) {
            super(fm);
        }

        public void addTab(Page page, Class<?> clss, Bundle args, String title) {
            TabInfo info = new TabInfo(page, clss, args, title);
            mTabs.add(info);
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            return mTabs.size();
        }

        @Override
        public Fragment getItem(int position) {
            TabInfo info = mTabs.get(position);
            return Fragment.instantiate(mContext, info.clss.getName(), info.args);
        }

        @Override
        public CharSequence getPageTitle(int position) {
            TabInfo info = mTabs.get(position);
            return info.title.toUpperCase();
        }

        @Override
        public Object instantiateItem(ViewGroup container, int position) {
            Fragment fragment = (Fragment) super.instantiateItem(container, position);

            mPages.put(mTabs.get(position).page, fragment);

            return fragment;
        }

        @Override
        public void destroyItem(ViewGroup container, int position, Object object) {
            super.destroyItem(container, position, object);

            mPages.remove(mTabs.get(position).page);
        }
    }
}
