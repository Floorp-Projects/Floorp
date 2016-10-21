/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.themed.ThemedImageButton;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.AppCompatDrawableManager;
import android.util.AttributeSet;

public class MenuItemActionBar extends ThemedImageButton
                               implements GeckoMenuItem.Layout {
    private static final String LOGTAG = "GeckoMenuItemActionBar";

    public MenuItemActionBar(Context context) {
        this(context, null);
    }

    public MenuItemActionBar(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemActionBarStyle);
    }

    public MenuItemActionBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public void initialize(GeckoMenuItem item) {
        if (item == null)
            return;

        setIcon(item.getIcon());
        setTitle(item.getTitle());
        setEnabled(item.isEnabled());
        setId(item.getItemId());
    }

    void setIcon(Drawable icon) {
        if (icon == null) {
            setVisibility(GONE);
        } else {
            setVisibility(VISIBLE);
            setImageDrawable(icon);
        }
    }

    void setIcon(int icon) {
        setIcon((icon == 0) ? null : AppCompatDrawableManager.get().getDrawable(getContext(), icon));
    }

    void setTitle(CharSequence title) {
        // set accessibility contentDescription here
        setContentDescription(title);
    }

    @Override
    public void setShowIcon(boolean show) {
        // Do nothing.
    }
}
