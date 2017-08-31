/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.TextView;

import org.mozilla.gecko.R;

public class StreamTitleRow extends StreamViewHolder {
    public static final int LAYOUT_ID = R.layout.activity_stream_main_highlightstitle;

    public StreamTitleRow(final View itemView, final @StringRes @NonNull int titleResId, boolean isEnabled) {
        super(itemView);
        final TextView titleView = (TextView) itemView.findViewById(R.id.title_highlights);
        titleView.setText(titleResId);
        if (!isEnabled) {
            hideView(itemView);
        }
    }

    private static void hideView(final View itemView) {
        itemView.setVisibility(View.GONE);
        // We also need to set the layout height, width, and margins to 0 for the RecyclerView child.
        final RecyclerView.LayoutParams layoutParams = (RecyclerView.LayoutParams) itemView.getLayoutParams();
        layoutParams.setMargins(0, 0, 0, 0);
        layoutParams.height = 0;
        layoutParams.width = 0;
        itemView.setLayoutParams(layoutParams);
    }
}

