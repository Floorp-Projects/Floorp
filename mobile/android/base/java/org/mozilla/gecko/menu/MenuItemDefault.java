/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.TextView;

public class MenuItemDefault extends TextView
                             implements GeckoMenuItem.Layout {
    private static final int[] STATE_MORE = new int[] { R.attr.state_more };
    private static final int[] STATE_CHECKED = new int[] { android.R.attr.state_checkable, android.R.attr.state_checked };
    private static final int[] STATE_UNCHECKED = new int[] { android.R.attr.state_checkable };

    private Drawable mIcon;
    private final Drawable mState;
    private static Rect sIconBounds;

    private boolean mCheckable;
    private boolean mChecked;
    private boolean mHasSubMenu;
    private boolean mShowIcon;

    public MenuItemDefault(Context context) {
        this(context, null);
    }

    public MenuItemDefault(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemDefaultStyle);
    }

    public MenuItemDefault(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        Resources res = getResources();
        int width = res.getDimensionPixelSize(R.dimen.menu_item_row_width);
        int height = res.getDimensionPixelSize(R.dimen.menu_item_row_height);
        setMinimumWidth(width);
        setMinimumHeight(height);

        int stateIconSize = res.getDimensionPixelSize(R.dimen.menu_item_state_icon);
        Rect stateIconBounds = new Rect(0, 0, stateIconSize, stateIconSize);

        mState = res.getDrawable(R.drawable.menu_item_state).mutate();
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
    public void initialize(GeckoMenuItem item) {
        if (item == null)
            return;

        setTitle(item.getTitle());
        setIcon(item.getIcon());
        setEnabled(item.isEnabled());
        setCheckable(item.isCheckable());
        setChecked(item.isChecked());
        setSubMenuIndicator(item.hasSubMenu());
    }

    private void refreshIcon() {
        setCompoundDrawables(mShowIcon ? mIcon : null, null, mState, null);
    }

    void setIcon(Drawable icon) {
        mIcon = icon;

        if (mIcon != null) {
            mIcon.setBounds(sIconBounds);
            mIcon.setAlpha(isEnabled() ? 255 : 99);
        }

        refreshIcon();
    }

    void setIcon(int icon) {
        setIcon((icon == 0) ? null : getResources().getDrawable(icon));
    }

    void setTitle(CharSequence title) {
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

    private void setCheckable(boolean checkable) {
        if (mCheckable != checkable) {
            mCheckable = checkable;
            refreshDrawableState();
        }
    }

    private void setChecked(boolean checked) {
        if (mChecked != checked) {
            mChecked = checked;
            refreshDrawableState();
        }
    }

    @Override
    public void setShowIcon(boolean show) {
        if (mShowIcon != show) {
            mShowIcon = show;
            refreshIcon();
        }
    }

    void setSubMenuIndicator(boolean hasSubMenu) {
        if (mHasSubMenu != hasSubMenu) {
            mHasSubMenu = hasSubMenu;
            refreshDrawableState();
        }
    }
}
