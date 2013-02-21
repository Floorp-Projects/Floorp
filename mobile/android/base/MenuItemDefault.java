/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.AbsListView;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MenuItemDefault extends LinearLayout
                             implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemDefault";
    private static Rect sIconBounds;

    private TextView mTitle;
    private CheckBox mCheck;
    private ImageView mMore;

    private Drawable mIcon;
    private boolean mCheckable;
    private boolean mChecked;
    private boolean mHasSubMenu;

    public MenuItemDefault(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources res = context.getResources();
        setLayoutParams(new AbsListView.LayoutParams((int) (res.getDimension(R.dimen.menu_item_row_width)),
                                                     (int) (res.getDimension(R.dimen.menu_item_row_height))));

        inflate(context, R.layout.menu_item, this);
        mTitle = (TextView) findViewById(R.id.title);
        mCheck = (CheckBox) findViewById(R.id.check);
        mMore = (ImageView) findViewById(R.id.more);

        mCheckable = false;
        mChecked = false;
        mHasSubMenu = false;

        if (sIconBounds == null) {
            int iconSize = res.getDimensionPixelSize(R.dimen.menu_item_icon);
            sIconBounds = new Rect(0, 0, iconSize, iconSize);
        }
    }

    @Override
    public View getLayout() {
        return this;
    }

    @Override
    public void setIcon(Drawable icon) {
        mIcon = icon;

        if (mIcon != null)
            mIcon.setBounds(sIconBounds);

        mTitle.setCompoundDrawables(mIcon, null, null, null);
    }

    @Override
    public void setIcon(int icon) {
        Drawable drawable = null;

        if (icon != 0)
            drawable = getContext().getResources().getDrawable(icon);
         
        setIcon(drawable);
    }

    @Override
    public void setTitle(CharSequence title) {
        mTitle.setText(title);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mTitle.setEnabled(enabled);

        if (mIcon != null)
            mIcon.setAlpha(enabled ? 255 : 99);

        mCheck.setEnabled(enabled);
        mMore.setColorFilter(enabled ? 0 : 0xFF999999);
    }

    @Override
    public void setCheckable(boolean checkable) {
        mCheckable = checkable;
        mCheck.setVisibility(mCheckable && !mHasSubMenu ? VISIBLE : GONE);
    }

    @Override
    public void setChecked(boolean checked) {
        mChecked = checked;
        mCheck.setChecked(mChecked);
    }

    @Override
    public void setSubMenuIndicator(boolean hasSubMenu) {
        mHasSubMenu = hasSubMenu;
        mMore.setVisibility(mHasSubMenu ? VISIBLE : GONE);
        mCheck.setVisibility(mCheckable && !mHasSubMenu ? VISIBLE : GONE);
    }
}
