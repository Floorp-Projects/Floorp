/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.firstrun;

import android.content.Context;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewPager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;

public class FirstrunPagerAdapter extends PagerAdapter {
    private static final int[] PAGE_LAYOUTS = {
            R.layout.item_firstrun_page1,
            R.layout.item_firstrun_page2,
            R.layout.item_firstrun_page3,
    };

    private Context context;
    private View.OnClickListener listener;

    public FirstrunPagerAdapter(Context context, View.OnClickListener listener) {
        this.context = context;
        this.listener = listener;
    }

    private View getView(int position, ViewPager pager) {
        final View view = LayoutInflater.from(context).inflate(
                PAGE_LAYOUTS[position],
                pager,
                false);

        final View nextView = view.findViewById(R.id.next);
        if (nextView != null) {
            nextView.setOnClickListener(listener);
        }

        final View finishView = view.findViewById(R.id.finish);
        if (finishView != null) {
            finishView.setOnClickListener(listener);
        }

        return view;
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return view == object;
    }

    @Override
    public int getCount() {
        return PAGE_LAYOUTS.length;
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        ViewPager pager = (ViewPager) container;
        View view = getView(position, pager);

        pager.addView(view);

        return view;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object view) {
        container.removeView((View) view);
    }
}
