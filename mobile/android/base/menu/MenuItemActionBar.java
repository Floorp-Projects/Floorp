/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;

public class MenuItemActionBar extends ImageButton
                               implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemActionBar";

    public MenuItemActionBar(Context context, AttributeSet attrs) {
        super(context, attrs);

        int size = (int) (context.getResources().getDimension(R.dimen.browser_toolbar_height));
        setLayoutParams(new ViewGroup.LayoutParams(size, size));

        int padding = (int) (context.getResources().getDimension(R.dimen.browser_toolbar_button_padding));
        setPadding(padding, padding, padding, padding);
        setBackgroundResource(R.drawable.action_bar_button);
        setScaleType(ImageView.ScaleType.FIT_CENTER);
    }

    @Override
    public View getView() {
        return this;
    }

    @Override
    public void setIcon(Drawable icon) {
        if (icon != null) {
            setImageDrawable(icon);
            setVisibility(VISIBLE);
        } else {
            setVisibility(GONE);
        }
    }

    @Override
    public void setIcon(int icon) {
        if (icon != 0) {
            setImageResource(icon);
            setVisibility(VISIBLE);
        } else {
            setVisibility(GONE);
        }
    }

    @Override
    public void setTitle(CharSequence title) {
        // set accessibility contentDescription here
        setContentDescription(title);
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        setColorFilter(enabled ? 0 : 0xFF999999);
    }

    @Override
    public void setCheckable(boolean checkable) {
    }

    @Override
    public void setChecked(boolean checked) {
    }

    @Override
    public void setSubMenuIndicator(boolean hasSubMenu) {
    }
}
