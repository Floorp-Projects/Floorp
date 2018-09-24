/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.TextView;

import org.mozilla.focus.R;

/* package-private */ class MenuItemViewHolder extends BrowserMenuViewHolder {
    /* package-private */ static final int LAYOUT_ID = R.layout.menu_item;

    private final TextView menuItemView;

    /* package-private */ MenuItemViewHolder(View itemView) {
        super(itemView);

        menuItemView = (TextView) itemView;
    }

    /* package-private */ void bind(BrowserMenuAdapter.MenuItem.Default menuItem) {
        menuItemView.setId(menuItem.getId());
        menuItemView.setText(menuItem.getLabel());
        if (menuItem.getDrawableResId() != 0) {
            final Drawable drawable = menuItemView.getContext().getDrawable(menuItem.getDrawableResId());
            if (drawable != null) {
                drawable.setTint(menuItemView.getContext().getResources().getColor(R.color.colorSettingsTint));
                menuItemView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                        drawable,
                        null,
                        null,
                        null
                );
            }
        }

        final boolean isLoading = browserFragment.getSession().getLoading();

        if ((menuItem.getId() == R.id.add_to_homescreen || menuItem.getId() == R.id.find_in_page) && isLoading) {
            menuItemView.setTextColor(browserFragment.getResources().getColor(R.color.colorTextInactive));
            menuItemView.setClickable(false);
        } else {
            menuItemView.setOnClickListener(this);
        }
    }
}
