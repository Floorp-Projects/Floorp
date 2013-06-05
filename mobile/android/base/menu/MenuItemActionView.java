/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ImageButton;

public class MenuItemActionView extends LinearLayout
                                implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemActionView";

    private MenuItemDefault mMenuItem;
    private ImageButton mActionButton;

    public MenuItemActionView(Context context) {
        this(context, null);
    }

    public MenuItemActionView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemActionViewStyle);
    }

    @TargetApi(11)
    public MenuItemActionView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        Resources res = context.getResources();
        int width = res.getDimensionPixelSize(R.dimen.menu_item_row_width);
        int height = res.getDimensionPixelSize(R.dimen.menu_item_row_height);
        setMinimumWidth(width);
        setMinimumHeight(height);

        LayoutInflater.from(context).inflate(R.layout.menu_item_action_view, this);
        mMenuItem = (MenuItemDefault) findViewById(R.id.menu_item);
        mActionButton = (ImageButton) findViewById(R.id.action_button);
    }

    @Override
    public void initialize(GeckoMenuItem item) {
        if (item == null)
            return;

        setTitle(item.getTitle());
        setIcon(item.getIcon());
        setEnabled(item.isEnabled());
    }

    private void setIcon(Drawable icon) {
        mMenuItem.setIcon(icon);
    }

    private void setIcon(int icon) {
        mMenuItem.setIcon(icon);
    }

    private void setTitle(CharSequence title) {
        mMenuItem.setTitle(title);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mMenuItem.setEnabled(enabled);

        if (mActionButton != null) {
            mActionButton.setEnabled(enabled);
            mActionButton.setAlpha(enabled ? 255 : 99);
        }
    }

    public void setMenuItemClickListener(View.OnClickListener listener) {
        mMenuItem.setOnClickListener(listener);
    }

    public void setActionButtonClickListener(View.OnClickListener listener) {
        mActionButton.setOnClickListener(listener);
    }

    public void setActionButton(Drawable drawable) {
        mActionButton.setImageDrawable(drawable);
    }
}
