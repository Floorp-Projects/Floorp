/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.mozilla.focus.R;

public class AppViewHolder extends RecyclerView.ViewHolder {
    private final TextView titleView;
    private final ImageView iconView;

    public AppViewHolder(View itemView) {
        super(itemView);

        titleView = (TextView) itemView.findViewById(R.id.title);
        iconView = (ImageView) itemView.findViewById(R.id.icon);
    }

    public void bind(AppAdapter.App app) {
        titleView.setText(app.getLabel());

        iconView.setImageDrawable(app.loadIcon());
    }
}
