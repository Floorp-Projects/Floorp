/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.HomePager.Page;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentStatePagerAdapter;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.EnumMap;

class HomeAdapter extends FragmentStatePagerAdapter {

    private final Context mContext;
    private final ArrayList<TabInfo> mTabs;
    private final EnumMap<Page, Fragment> mPages;

    private OnAddTabListener mAddTabListener;

    interface OnAddTabListener {
        public void onAddTab(String title);
    }

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

    public HomeAdapter(Context context, FragmentManager fm) {
        super(fm);

        mContext = context;

        mTabs = new ArrayList<TabInfo>();
        mPages = new EnumMap<Page, Fragment>(Page.class);
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

    public void setOnAddTabListener(OnAddTabListener listener) {
        mAddTabListener = listener;
    }

    public void addTab(Page page, Class<?> clss, Bundle args, String title) {
        addTab(-1, page, clss, args, title);
    }

    public void addTab(int index, Page page, Class<?> clss, Bundle args, String title) {
        TabInfo info = new TabInfo(page, clss, args, title);

        if (index >= 0) {
            mTabs.add(index, info);
        } else {
            mTabs.add(info);
        }

        notifyDataSetChanged();

        if (mAddTabListener != null) {
            mAddTabListener.onAddTab(title);
        }
    }

    public int getItemPosition(Page page) {
        for (int i = 0; i < mTabs.size(); i++) {
            TabInfo info = mTabs.get(i);
            if (info.page == page) {
                return i;
            }
        }

        return -1;
    }

    public Page getPageAtPosition(int position) {
        TabInfo info = mTabs.get(position);
        return info.page;
    }

    public void setCanLoadHint(boolean canLoadHint) {
        // Update fragment arguments for future instances
        for (TabInfo info : mTabs) {
            info.args.putBoolean(HomePager.CAN_LOAD_ARG, canLoadHint);
        }

        // Enable/disable loading on all existing pages
        for (Fragment page : mPages.values()) {
            final HomeFragment homePage = (HomeFragment) page;
            homePage.setCanLoadHint(canLoadHint);
        }
    }
}
