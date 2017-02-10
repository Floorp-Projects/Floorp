/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.SettingsActivity;

/**
 * Fragment displaying the "home" screen when launching the app.
 */
public class HomeFragment extends Fragment implements View.OnClickListener {
    public static final String FRAGMENT_TAG = "home";

    public static HomeFragment create() {
        return new HomeFragment();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_home, container, false);

        view.findViewById(R.id.url).setOnClickListener(this);
        view.findViewById(R.id.settings).setOnClickListener(this);

        return view;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.url:
                final FragmentManager fragmentManager = getActivity().getSupportFragmentManager();
                fragmentManager.beginTransaction()
                        .add(R.id.container, UrlInputFragment.create())
                        .addToBackStack("url_input")
                        .commit();
                break;

            case R.id.settings:
                Intent intent = new Intent(getActivity(), SettingsActivity.class);
                startActivity(intent);
                break;
        }
    }
}
