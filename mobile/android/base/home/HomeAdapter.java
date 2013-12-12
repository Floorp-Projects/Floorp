/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.PageEntry;
import org.mozilla.gecko.home.HomeConfig.PageType;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.HomePager.Page;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentStatePagerAdapter;
import android.view.ViewGroup;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

class HomeAdapter extends FragmentStatePagerAdapter {

    private final Context mContext;
    private final ArrayList<PageInfo> mPageInfos;
    private final HashMap<String, Fragment> mPages;

    private boolean mCanLoadHint;

    private OnAddPageListener mAddPageListener;

    interface OnAddPageListener {
        public void onAddPage(String title);
    }

    public HomeAdapter(Context context, FragmentManager fm) {
        super(fm);

        mContext = context;
        mCanLoadHint = HomeFragment.DEFAULT_CAN_LOAD_HINT;

        mPageInfos = new ArrayList<PageInfo>();
        mPages = new HashMap<String, Fragment>();
    }

    @Override
    public int getCount() {
        return mPageInfos.size();
    }

    @Override
    public Fragment getItem(int position) {
        PageInfo info = mPageInfos.get(position);
        return Fragment.instantiate(mContext, info.getClassName(), info.getArgs());
    }

    @Override
    public CharSequence getPageTitle(int position) {
        if (mPageInfos.size() > 0) {
            PageInfo info = mPageInfos.get(position);
            return info.getTitle().toUpperCase();
        }

        return null;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        Fragment fragment = (Fragment) super.instantiateItem(container, position);
        mPages.put(mPageInfos.get(position).getId(), fragment);

        return fragment;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        super.destroyItem(container, position, object);
        mPages.remove(mPageInfos.get(position).getId());
    }

    public void setOnAddPageListener(OnAddPageListener listener) {
        mAddPageListener = listener;
    }

    public int getItemPosition(Page page) {
        for (int i = 0; i < mPageInfos.size(); i++) {
            final Page infoPage = mPageInfos.get(i).toPage();
            if (infoPage == page) {
                return i;
            }
        }

        return -1;
    }

    public Page getPageAtPosition(int position) {
        // getPageAtPosition() might be called before HomeAdapter
        // has got its initial list of PageEntries. Just bail.
        if (mPageInfos.isEmpty()) {
            return null;
        }

        PageInfo info = mPageInfos.get(position);
        return info.toPage();
    }

    private void addPage(PageInfo info) {
        mPageInfos.add(info);

        if (mAddPageListener != null) {
            mAddPageListener.onAddPage(info.getTitle());
        }
    }

    public void update(List<PageEntry> pageEntries) {
        mPages.clear();
        mPageInfos.clear();

        if (pageEntries != null) {
            for (PageEntry pageEntry : pageEntries) {
                final PageInfo info = new PageInfo(pageEntry);
                addPage(info);
            }
        }

        notifyDataSetChanged();
    }

    public boolean getCanLoadHint() {
        return mCanLoadHint;
    }

    public void setCanLoadHint(boolean canLoadHint) {
        // We cache the last hint value so that we can use it when
        // creating new pages. See PageInfo.getArgs().
        mCanLoadHint = canLoadHint;

        // Enable/disable loading on all existing pages
        for (Fragment page : mPages.values()) {
            final HomeFragment homePage = (HomeFragment) page;
            homePage.setCanLoadHint(canLoadHint);
        }
    }

    private final class PageInfo {
        private final String mId;
        private final PageEntry mPageEntry;

        PageInfo(PageEntry pageEntry) {
            mId = pageEntry.getType() + "-" + pageEntry.getId();
            mPageEntry = pageEntry;
        }

        public String getId() {
            return mId;
        }

        public String getTitle() {
            return mPageEntry.getTitle();
        }

        public String getClassName() {
            final PageType type = mPageEntry.getType();
            return type.getPageClass().getName();
        }

        public Bundle getArgs() {
            final Bundle args = new Bundle();

            args.putBoolean(HomePager.CAN_LOAD_ARG, mCanLoadHint);

            // Only list pages need the page entry
            if (mPageEntry.getType() == PageType.LIST) {
                args.putParcelable(HomePager.PAGE_ENTRY_ARG, mPageEntry);
            }

            return args;
        }

        public Page toPage() {
            final PageType type = mPageEntry.getType();
            if (type == PageType.LIST) {
                return null;
            }

            return Page.valueOf(type);
        }
    }
}
