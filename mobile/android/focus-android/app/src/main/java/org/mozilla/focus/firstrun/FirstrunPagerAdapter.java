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
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.focus.R;

public class FirstrunPagerAdapter extends PagerAdapter {
    private static class FirstrunPage {
        public final String title;
        public final String text;
        public final int imageResource;

        private FirstrunPage(String title, String text, int imageResource) {
            this.title = title;
            this.text = text;
            this.imageResource = imageResource;
        }
    }

    private final FirstrunPage[] pages;

    private Context context;
    private View.OnClickListener listener;

    public FirstrunPagerAdapter(Context context, View.OnClickListener listener) {
        this.context = context;
        this.listener = listener;
        this.pages = new FirstrunPage[] {
                new FirstrunPage(
                        context.getString(R.string.firstrun_tracking_title),
                        context.getString(R.string.firstrun_tracking_text),
                        R.drawable.onboarding_img1),
                new FirstrunPage(
                        context.getString(R.string.firstrun_defaultbrowser_title),
                        context.getString(R.string.firstrun_defaultbrowser_text, context.getString(R.string.launcher_name)),
                        R.drawable.onboarding_img2),
                new FirstrunPage(
                        context.getString(R.string.firstrun_breaking_title),
                        context.getString(R.string.firstrun_breaking_text),
                        R.drawable.onboarding_img3),
        };
    }

    private View getView(int position, ViewPager pager) {
        final View view = LayoutInflater.from(context).inflate(
                R.layout.firstrun_page, pager, false);

        final FirstrunPage page = pages[position];

        final TextView titleView = (TextView) view.findViewById(R.id.title);
        titleView.setText(page.title);

        final TextView textView = (TextView) view.findViewById(R.id.text);
        textView.setText(page.text);

        final ImageView imageView = (ImageView) view.findViewById(R.id.image);
        imageView.setImageResource(page.imageResource);

        final Button buttonView = (Button) view.findViewById(R.id.button);
        buttonView.setOnClickListener(listener);
        if (position == pages.length - 1) {
            buttonView.setText(R.string.firstrun_close_button);
            buttonView.setId(R.id.finish);
        } else {
            buttonView.setText(R.string.firstrun_next_button);
            buttonView.setId(R.id.next);
        }

        return view;
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return view == object;
    }

    @Override
    public int getCount() {
        return pages.length;
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
