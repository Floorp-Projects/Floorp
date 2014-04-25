/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.home.PanelLayout.FilterDetail;

import android.content.Context;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.squareup.picasso.Picasso;

class PanelBackItemView extends LinearLayout {
    private final TextView title;

    public PanelBackItemView(Context context, String backImageUrl) {
        super(context);

        LayoutInflater.from(context).inflate(R.layout.panel_back_item, this);
        setOrientation(HORIZONTAL);

        title = (TextView) findViewById(R.id.title);

        final ImageView image = (ImageView) findViewById(R.id.image);

        if (TextUtils.isEmpty(backImageUrl)) {
            image.setImageResource(R.drawable.folder_up);
        } else {
            Picasso.with(getContext())
                   .load(backImageUrl)
                   .placeholder(R.drawable.folder_up)
                   .into(image);
        }
    }

    public void updateFromFilter(FilterDetail filter) {
        final String backText = getResources()
            .getString(R.string.home_move_up_to_filter, filter.title);
        title.setText(backText);
    }
}
