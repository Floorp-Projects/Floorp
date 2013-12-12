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
    private final ArrayList<PageInfo> mPageInfos;
    private final EnumMap<Page, Fragment> mPages;

    private OnAddPageListener mAddPageListener;

    interface OnAddPageListener {
        public void onAddPage(String title);
    }

    final class PageInfo {
        private final Page page;
        private final Class<?> clss;
        private final Bundle args;
        private final String title;

        PageInfo(Page page, Class<?> clss, Bundle args, String title) {
            this.page = page;
            this.clss = clss;
            this.args = args;
            this.title = title;
        }
    }

    public HomeAdapter(Context context, FragmentManager fm) {
        super(fm);

        mContext = context;

        mPageInfos = new ArrayList<PageInfo>();
        mPages = new EnumMap<Page, Fragment>(Page.class);
    }

    @Override
    public int getCount() {
        return mPageInfos.size();
    }

    @Override
    public Fragment getItem(int position) {
        PageInfo info = mPageInfos.get(position);
        return Fragment.instantiate(mContext, info.clss.getName(), info.args);
    }

    @Override
    public CharSequence getPageTitle(int position) {
        PageInfo info = mPageInfos.get(position);
        return info.title.toUpperCase();
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        Fragment fragment = (Fragment) super.instantiateItem(container, position);
        mPages.put(mPageInfos.get(position).page, fragment);

        return fragment;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        super.destroyItem(container, position, object);
        mPages.remove(mPageInfos.get(position).page);
    }

    public void setOnAddPageListener(OnAddPageListener listener) {
        mAddPageListener = listener;
    }

    public void addPage(Page page, Class<?> clss, Bundle args, String title) {
        addPage(-1, page, clss, args, title);
    }

    public void addPage(int index, Page page, Class<?> clss, Bundle args, String title) {
        PageInfo info = new PageInfo(page, clss, args, title);

        if (index >= 0) {
            mPageInfos.add(index, info);
        } else {
            mPageInfos.add(info);
        }

        notifyDataSetChanged();

        if (mAddPageListener != null) {
            mAddPageListener.onAddPage(title);
        }
    }

    public int getItemPosition(Page page) {
        for (int i = 0; i < mPageInfos.size(); i++) {
            PageInfo info = mPageInfos.get(i);
            if (info.page == page) {
                return i;
            }
        }

        return -1;
    }

    public Page getPageAtPosition(int position) {
        PageInfo info = mPageInfos.get(position);
        return info.page;
    }

    public void setCanLoadHint(boolean canLoadHint) {
        // Update fragment arguments for future instances
        for (PageInfo info : mPageInfos) {
            info.args.putBoolean(HomePager.CAN_LOAD_ARG, canLoadHint);
        }

        // Enable/disable loading on all existing pages
        for (Fragment page : mPages.values()) {
            final HomeFragment homePage = (HomeFragment) page;
            homePage.setCanLoadHint(canLoadHint);
        }
    }
}
