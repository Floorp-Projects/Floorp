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
        public final String contentDescription;

        private FirstrunPage(String title, String text, int imageResource) {
            this.title = title;
            this.text = text;
            this.imageResource = imageResource;
            this.contentDescription = title + text;
        }
    }

    private final FirstrunPage[] pages;

    private final Context context;
    private final View.OnClickListener listener;

    public FirstrunPagerAdapter(Context context, View.OnClickListener listener) {
        final String appName = context.getString(R.string.app_name);

        this.context = context;
        this.listener = listener;
        this.pages = new FirstrunPage[] {
                new FirstrunPage(
                        context.getString(R.string.firstrun_defaultbrowser_title),
                        context.getString(R.string.firstrun_defaultbrowser_text2),
                        R.drawable.onboarding_img1),
                new FirstrunPage(
                        context.getString(R.string.firstrun_search_title),
                        context.getString(R.string.firstrun_search_text),
                        R.drawable.onboarding_img4),
                new FirstrunPage(
                        context.getString(R.string.firstrun_shortcut_title),
                        context.getString(R.string.firstrun_shortcut_text, appName),
                        R.drawable.onboarding_img3),
                new FirstrunPage(
                        context.getString(R.string.firstrun_privacy_title),
                        context.getString(R.string.firstrun_privacy_text, appName),
                        R.drawable.onboarding_img2)
        };
    }

    private View getView(int position, ViewPager pager) {
        final View view = LayoutInflater.from(context).inflate(
                R.layout.firstrun_page, pager, false);

        final FirstrunPage page = pages[position];

        final TextView titleView = view.findViewById(R.id.title);
        titleView.setText(page.title);

        final TextView textView = view.findViewById(R.id.text);
        textView.setText(page.text);

        final ImageView imageView = view.findViewById(R.id.image);
        imageView.setImageResource(page.imageResource);

        final Button buttonView = view.findViewById(R.id.button);
        buttonView.setOnClickListener(listener);
        if (position == pages.length - 1) {
            buttonView.setText(R.string.firstrun_close_button);
            buttonView.setId(R.id.finish);
            buttonView.setContentDescription(buttonView.getText().toString().toLowerCase());
        } else {
            buttonView.setText(R.string.firstrun_next_button);
            buttonView.setId(R.id.next);
        }

        return view;
    }

    public String getPageAccessibilityDescription(int position) {
        return pages[position].contentDescription;
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
