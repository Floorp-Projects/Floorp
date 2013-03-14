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
import android.widget.TextView;

public class MenuItemDefault extends TextView
                             implements GeckoMenuItem.Layout {
    private static final int[] STATE_MORE = new int[] { R.attr.state_more };
    private static final int[] STATE_CHECKED = new int[] { android.R.attr.state_checkable, android.R.attr.state_checked };
    private static final int[] STATE_UNCHECKED = new int[] { android.R.attr.state_checkable };

    private Drawable mIcon;
    private Drawable mState;
    private static Rect sIconBounds;

    private boolean mCheckable = false;
    private boolean mChecked = false;
    private boolean mHasSubMenu = false;

    public MenuItemDefault(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources res = context.getResources();
        int stateIconSize = res.getDimensionPixelSize(R.dimen.menu_item_state_icon);
        Rect stateIconBounds = new Rect(0, 0, stateIconSize, stateIconSize);

        mState = res.getDrawable(R.drawable.menu_item_state);
        mState.setBounds(stateIconBounds);

        if (sIconBounds == null) {
            int iconSize = res.getDimensionPixelSize(R.dimen.menu_item_icon);
            sIconBounds = new Rect(0, 0, iconSize, iconSize);
        }

        setCompoundDrawables(mIcon, null, mState, null);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 2);

        if (mHasSubMenu)
            mergeDrawableStates(drawableState, STATE_MORE);
        else if (mCheckable && mChecked)
            mergeDrawableStates(drawableState, STATE_CHECKED);
        else if (mCheckable && !mChecked)
            mergeDrawableStates(drawableState, STATE_UNCHECKED);

        return drawableState;
    }

    @Override
    public View getView() {
        return this;
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
        if (mCheckable != checkable) {
            mCheckable = checkable;
            refreshDrawableState();
        }
    }

    @Override
    public void setChecked(boolean checked) {
        if (mChecked != checked) {
            mChecked = checked;
            refreshDrawableState();
        }
    }

    @Override
    public void setSubMenuIndicator(boolean hasSubMenu) {
        if (mHasSubMenu != hasSubMenu) {
            mHasSubMenu = hasSubMenu;
            refreshDrawableState();
        }
    }
}
