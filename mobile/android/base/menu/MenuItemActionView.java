/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.R;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

public class MenuItemActionView extends LinearLayout
                                implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemActionView";

    private MenuItemDefault mMenuItem;
    private MenuItemActionBar mMenuButton;
    private List<ImageButton> mActionButtons;
    private View.OnClickListener mActionButtonListener;

    public MenuItemActionView(Context context) {
        this(context, null);
    }

    public MenuItemActionView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemActionViewStyle);
    }

    @TargetApi(11)
    public MenuItemActionView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.menu_item_action_view, this);
        mMenuItem = (MenuItemDefault) findViewById(R.id.menu_item);
        mMenuButton = (MenuItemActionBar) findViewById(R.id.menu_item_button);
        mActionButtons = new ArrayList<ImageButton>();
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        View parent = (View) getParent();
        if ((right - left) < parent.getMeasuredWidth() || mActionButtons.size() != 0) {
            // Use the icon.
            mMenuItem.setVisibility(View.GONE);
            mMenuButton.setVisibility(View.VISIBLE);
        } else {
            // Use the button.
            mMenuItem.setVisibility(View.VISIBLE);
            mMenuButton.setVisibility(View.GONE);
        }

        super.onLayout(changed, left, top, right, bottom);
    }

    @Override
    public void initialize(GeckoMenuItem item) {
        if (item == null)
            return;

        mMenuItem.initialize(item);
        mMenuButton.initialize(item);
        setEnabled(item.isEnabled());
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mMenuItem.setEnabled(enabled);
        mMenuButton.setEnabled(enabled);

        for (ImageButton button : mActionButtons) {
             button.setEnabled(enabled);
             button.setAlpha(enabled ? 255 : 99);
        }
    }

    public void setMenuItemClickListener(View.OnClickListener listener) {
        mMenuItem.setOnClickListener(listener);
        mMenuButton.setOnClickListener(listener);
    }

    public void setActionButtonClickListener(View.OnClickListener listener) {
        mActionButtonListener = listener;

        for (ImageButton button : mActionButtons) {
            button.setOnClickListener(listener);
        }
    }

    @Override
    public void setShowIcon(boolean show) {
        mMenuItem.setShowIcon(show);
    }

    public void setIcon(Drawable icon) {
        mMenuItem.setIcon(icon);
        mMenuButton.setIcon(icon);
    }

    public void setIcon(int icon) {
        mMenuItem.setIcon(icon);
        mMenuButton.setIcon(icon);
    }

    public void setTitle(CharSequence title) {
        mMenuItem.setTitle(title);
        mMenuButton.setContentDescription(title);
    }

    public void setSubMenuIndicator(boolean hasSubMenu) {
        mMenuItem.setSubMenuIndicator(hasSubMenu);
    }

    public void addActionButton(Drawable drawable) {
        // If this is the first icon, retain the text.
        // If not, make the menu item an icon.
        final int count = mActionButtons.size();
        mMenuItem.setVisibility(View.GONE);
        mMenuButton.setVisibility(View.VISIBLE);

        if (drawable != null) {
            ImageButton button = new ImageButton(getContext(), null, R.attr.menuItemShareActionButtonStyle);
            button.setImageDrawable(drawable);
            button.setOnClickListener(mActionButtonListener);
            button.setTag(count);

            final int height = (int) (getResources().getDimension(R.dimen.menu_item_row_height));
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, height);
            params.weight = 1.0f;
            button.setLayoutParams(params);

            // Fill in the action-buttons to the left of the actual menu button.
            mActionButtons.add(button);
            addView(button, count);
        }
    }
}
