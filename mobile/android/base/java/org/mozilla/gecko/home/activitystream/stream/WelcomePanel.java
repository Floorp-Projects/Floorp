/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home.activitystream.stream;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewStub;
import android.widget.Button;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;

public class WelcomePanel extends StreamItem implements View.OnClickListener {
    public static final int LAYOUT_ID = R.layout.activity_stream_main_welcomepanel;

    public static final String PREF_WELCOME_DISMISSED = "activitystream.welcome_dismissed";

    private final RecyclerView.Adapter<StreamItem> adapter;
    private final Context context;

    public WelcomePanel(final View itemView, final RecyclerView.Adapter<StreamItem> adapter) {
        super(itemView);

        this.adapter = adapter;
        this.context = itemView.getContext();

        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(itemView.getContext());

        if (!sharedPrefs.getBoolean(PREF_WELCOME_DISMISSED, false)) {
            final ViewStub welcomePanelStub = (ViewStub) itemView.findViewById(R.id.welcomepanel_stub);

            welcomePanelStub.inflate();

            final Button dismissButton = (Button) itemView.findViewById(R.id.dismiss_welcomepanel);

            dismissButton.setOnClickListener(this);
        }
    }

    @Override
    public void onClick(View v) {
        // To animate between item changes, RecyclerView keeps around the old version of the view,
        // and creates a new equivalent item (which is bound using the new data) - followed by
        // animating between those two versions. Hence we just need to make sure that
        // any future calls to onCreateViewHolder create a version of the Header Item
        // with the welcome panel hidden (i.e. we don't need to care about animations ourselves).
        // We communicate this state change via the pref.

        final SharedPreferences sharedPrefs = GeckoSharedPrefs.forApp(context);

        sharedPrefs.edit()
                .putBoolean(WelcomePanel.PREF_WELCOME_DISMISSED, true)
                .apply();

        adapter.notifyItemChanged(getAdapterPosition());
    }
}