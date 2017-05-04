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
import android.support.v4.app.Fragment;
import android.support.v4.view.ViewPager;
import android.transition.Transition;
import android.transition.TransitionInflater;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;
import org.mozilla.focus.firstrun.FirstrunPagerAdapter;

public class FirstrunFragment extends Fragment implements View.OnClickListener {
    public static final String FRAGMENT_TAG = "firstrun";

    public static final String FIRSTRUN_PREF = "firstrun_shown";

    public static FirstrunFragment create() {
        return new FirstrunFragment();
    }

    private ViewPager viewPager;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        final Transition transition = TransitionInflater.from(context).
                inflateTransition(R.transition.firstrun_exit);

        setExitTransition(transition);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_firstrun, container, false);

        final View background = view.findViewById(R.id.background);
        final FirstrunPagerAdapter adapter = new FirstrunPagerAdapter(container.getContext(), this);

        viewPager = (ViewPager) view.findViewById(R.id.pager);
        // TODO: Those values are only for testing - needs to be adopted to final mock
        viewPager.setPadding(100, 0, 100, 0);
        viewPager.setPageMargin(-50);
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

        return view;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.next:
                viewPager.setCurrentItem(viewPager.getCurrentItem() + 1);
                break;

            case R.id.finish:
                PreferenceManager.getDefaultSharedPreferences(getContext())
                        .edit()
                        .putBoolean(FIRSTRUN_PREF, true)
                        .apply();

                ((MainActivity) getActivity()).firstrunFinished();
                break;
        }
    }
}
