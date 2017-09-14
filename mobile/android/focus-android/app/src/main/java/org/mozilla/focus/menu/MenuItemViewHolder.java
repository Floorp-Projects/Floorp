/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu;

import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;

/* package-private */ class MenuItemViewHolder extends BrowserMenuViewHolder {
    /* package-private */ static final int LAYOUT_ID = R.layout.menu_item;

    private TextView menuItemView;

    /* package-private */ MenuItemViewHolder(View itemView) {
        super(itemView);

        menuItemView = (TextView) itemView;
    }

    /* package-private */ void bind(BrowserMenuAdapter.MenuItem menuItem) {
        menuItemView.setId(menuItem.id);
        menuItemView.setText(menuItem.label);

        final boolean isLoading = browserFragment.getSession().getLoading().getValue();

        if (menuItem.id == R.id.add_to_homescreen && isLoading) {
            menuItemView.setTextColor(browserFragment.getResources().getColor(R.color.colorTextInactive));
            menuItemView.setClickable(false);
        } else {
            menuItemView.setOnClickListener(this);
        }
    }
}
