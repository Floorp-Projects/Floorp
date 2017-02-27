/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.fragment;

import android.content.Context;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.MainActivity;

public class FirstrunFragment extends Fragment implements View.OnClickListener {
    public static final String FRAGMENT_TAG = "firstrun";

    public static final String FIRSTRUN_PREF = "firstrun_shown";

    public static FirstrunFragment create() {
        return new FirstrunFragment();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_firstrun, container, false);

        view.findViewById(R.id.firstrun_exitbutton).setOnClickListener(this);

        return view;
    }

    @Override
    public void onClick(View v) {
        PreferenceManager.getDefaultSharedPreferences(getContext())
                .edit()
                .putBoolean(FIRSTRUN_PREF, true)
                .commit();

        ((MainActivity) getActivity()).firstrunFinished();
    }
}
