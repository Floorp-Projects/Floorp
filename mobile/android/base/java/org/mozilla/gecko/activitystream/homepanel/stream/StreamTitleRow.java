/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.stream;

import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.activitystream.ActivityStreamTelemetry;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.util.DrawableUtil;

import java.util.EnumSet;

public class StreamTitleRow extends StreamViewHolder {
    public static final int LAYOUT_ID = R.layout.activity_stream_main_highlightstitle;

    public StreamTitleRow(final View itemView, final @StringRes @NonNull int titleResId) {
        super(itemView);
        final TextView titleView = (TextView) itemView.findViewById(R.id.title_highlights);
        titleView.setText(titleResId);
    }

    public StreamTitleRow(final View itemView, final @StringRes @NonNull int titleResId,
                          final @StringRes int linkTitleResId, final String url, final HomePager.OnUrlOpenListener onUrlOpenListener) {
        this(itemView, titleResId);
        // Android 21+ is needed to set RTL-aware compound drawables, so we use a tinted ImageView here.
        final TextView titleLink = (TextView) itemView.findViewById(R.id.title_link);
        titleLink.setVisibility(View.VISIBLE);
        titleLink.setText(linkTitleResId);

        final ImageView titleArrow = (ImageView) itemView.findViewById(R.id.arrow_link);
        titleArrow.setImageDrawable(DrawableUtil.tintDrawableWithColorRes(itemView.getContext(), R.drawable.menu_item_more, R.color.ob_click));
        titleArrow.setVisibility(View.VISIBLE);

        final View.OnClickListener clickListener = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onUrlOpenListener.onUrlOpen(url, EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));

                ActivityStreamTelemetry.Extras.Builder extras = ActivityStreamTelemetry.Extras.builder()
                        .set(ActivityStreamTelemetry.Contract.SOURCE_TYPE, ActivityStreamTelemetry.Contract.TYPE_POCKET)
                        .set(ActivityStreamTelemetry.Contract.ITEM, ActivityStreamTelemetry.Contract.ITEM_LINK_MORE);

                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, extras.build());
            }
        };

        titleLink.setOnClickListener(clickListener);
        titleArrow.setOnClickListener(clickListener);
    }
}

