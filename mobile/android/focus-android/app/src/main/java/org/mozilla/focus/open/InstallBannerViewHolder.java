/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.ImageView;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InstallFirefoxActivity;

public class InstallBannerViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
    public static final int LAYOUT_ID = R.layout.item_install_banner;

    private final ImageView iconView;

    public InstallBannerViewHolder(View itemView) {
        super(itemView);

        iconView = itemView.findViewById(R.id.icon);

        itemView.setOnClickListener(this);
    }

    public void bind(AppAdapter.App store) {
        iconView.setImageDrawable(store.loadIcon());
    }

    @Override
    public void onClick(View view) {
        InstallFirefoxActivity.Companion.open(view.getContext());
    }
}
