/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.fragment;

import android.content.Context;
import android.graphics.drawable.TransitionDrawable;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.design.widget.TabLayout;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.view.ViewPager;
import android.transition.Transition;
import android.transition.TransitionInflater;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.firstrun.FirstrunPagerAdapter;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.StatusBarUtils;

public class FirstrunFragment extends Fragment implements View.OnClickListener {
    public static final String FRAGMENT_TAG = "firstrun";

    public static final String FIRSTRUN_PREF = "firstrun_shown";

    public static FirstrunFragment create() {
        return new FirstrunFragment();
    }

    private ViewPager viewPager;

    private View background;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        final Transition transition = TransitionInflater.from(context).
                inflateTransition(R.transition.firstrun_exit);

        setExitTransition(transition);

        // We will send a telemetry event whenever a new firstrun page is shown. However this page
        // listener won't fire for the initial page we are showing. So we are going to firing here.
        TelemetryWrapper.showFirstRunPageEvent(0);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_firstrun, container, false);

        view.findViewById(R.id.skip).setOnClickListener(this);

        background = view.findViewById(R.id.background);

        final FirstrunPagerAdapter adapter = new FirstrunPagerAdapter(container.getContext(), this);

        viewPager = (ViewPager) view.findViewById(R.id.pager);
        viewPager.setFocusable(true);

        viewPager.setPageTransformer(true, new ViewPager.PageTransformer() {
            @Override
            public void transformPage(View page, float position) {
                page.setAlpha(1 - (0.5f * Math.abs(position)));
            }
        });
        viewPager.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageSelected(int position) {
                TelemetryWrapper.showFirstRunPageEvent(position);
            }

            @Override
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {}

            @Override
            public void onPageScrollStateChanged(int state) {}
        });

        viewPager.setClipToPadding(false);
        viewPager.setAdapter(adapter);
        viewPager.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageSelected(int position) {
                final TransitionDrawable drawable = (TransitionDrawable) background.getBackground();

                if (position == adapter.getCount() - 1) {
                    drawable.startTransition(200);
                } else {
                    drawable.resetTransition();
                }
            }

            @Override
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {}

            @Override
            public void onPageScrollStateChanged(int state) {}
        });

        final TabLayout tabLayout = (TabLayout) view.findViewById(R.id.tabs);
        tabLayout.setupWithViewPager(viewPager, true);

        return view;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.next:
                viewPager.setCurrentItem(viewPager.getCurrentItem() + 1);
                break;

            case R.id.skip:
                finishFirstrun();
                TelemetryWrapper.skipFirstRunEvent();
                break;

            case R.id.finish:
                finishFirstrun();
                TelemetryWrapper.finishFirstRunEvent();
                break;

            default:
                throw new IllegalArgumentException("Unknown view");
        }
    }

    private void finishFirstrun() {
        final FragmentManager fragmentManager = getActivity().getSupportFragmentManager();

        PreferenceManager.getDefaultSharedPreferences(getContext())
                .edit()
                .putBoolean(FIRSTRUN_PREF, true)
                .apply();

        fragmentManager
                .beginTransaction()
                .remove(this)
                .commit();

        final UrlInputFragment inputFragment = (UrlInputFragment) fragmentManager.findFragmentByTag(UrlInputFragment.FRAGMENT_TAG);
        if (inputFragment != null) {
            inputFragment.showKeyboard();
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        StatusBarUtils.getStatusBarHeight(background, new StatusBarUtils.StatusBarHeightListener() {
            @Override
            public void onStatusBarHeightFetched(int statusBarHeight) {
                background.setPadding(0, statusBarHeight, 0, 0);
            }
        });
    }
}
