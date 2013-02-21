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
import android.widget.TextView;

public class MenuItemDefault extends TextView
                             implements GeckoMenuItem.Layout {
    private static Rect sIconBounds;
    private static Drawable sChecked;
    private static Drawable sUnChecked;
    private static Drawable sMore;

    private Drawable mIcon;
    private Drawable mState;

    private boolean mCheckable = false;
    private boolean mChecked = false;
    private boolean mHasSubMenu = false;

    public MenuItemDefault(Context context, AttributeSet attrs) {
        super(context, attrs);

        if (sIconBounds == null) {
            Resources res = context.getResources();
            int iconSize = res.getDimensionPixelSize(R.dimen.menu_item_icon);
            sIconBounds = new Rect(0, 0, iconSize, iconSize);

            int stateIconSize = res.getDimensionPixelSize(R.dimen.menu_item_state_icon);
            Rect stateIconBounds = new Rect(0, 0, stateIconSize, stateIconSize);

            sChecked = res.getDrawable(R.drawable.menu_item_check);
            sChecked.setBounds(stateIconBounds);

            sUnChecked = res.getDrawable(R.drawable.menu_item_uncheck);
            sUnChecked.setBounds(stateIconBounds);

            sMore = res.getDrawable(R.drawable.menu_item_more);
            sMore.setBounds(stateIconBounds);
        }
    }

    @Override
    public void setIcon(Drawable icon) {
        mIcon = icon;

        if (mIcon != null)
            mIcon.setBounds(sIconBounds);

        setCompoundDrawables(mIcon, null, mState, null);
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
        setText(title);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);

        if (mIcon != null)
            mIcon.setAlpha(enabled ? 255 : 99);

        if (mState != null)
            mState.setAlpha(enabled ? 255 : 99);
    }

    @Override
    public void setCheckable(boolean checkable) {
        mCheckable = checkable;
        refreshState();
    }

    private void refreshState() {
        if (mHasSubMenu)
            mState = sMore;
        else if (mCheckable && mChecked)
            mState = sChecked;
        else if (mCheckable && !mChecked)
            mState = sUnChecked;
        else
            mState = null;

        setCompoundDrawables(mIcon, null, mState, null);
    }

    @Override
    public void setChecked(boolean checked) {
        mChecked = checked;
        refreshState();
    }

    @Override
    public void setSubMenuIndicator(boolean hasSubMenu) {
        mHasSubMenu = hasSubMenu;
        mState = sMore;
        refreshState();
    }
}
