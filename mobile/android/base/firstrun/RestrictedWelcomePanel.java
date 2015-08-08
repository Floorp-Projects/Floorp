/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.firstrun;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.home.HomePager;

import android.app.Activity;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import java.util.EnumSet;

public class RestrictedWelcomePanel extends FirstrunPanel {
    public static final int TITLE_RES = R.string.firstrun_panel_title_welcome;

    private static final String LEARN_MORE_URL = "https://support.mozilla.org/kb/kids";

    private HomePager.OnUrlOpenListener onUrlOpenListener;

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            onUrlOpenListener = (HomePager.OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString() + " must implement HomePager.OnUrlOpenListener");
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        final ViewGroup root = (ViewGroup) inflater.inflate(R.layout.restricted_firstrun_welcome_fragment, container, false);

        root.findViewById(R.id.welcome_browse).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                close();
            }
        });

        root.findViewById(R.id.learn_more_link).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onUrlOpenListener.onUrlOpen(LEARN_MORE_URL, EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));

                close();
            }
        });

        return root;
    }
}
