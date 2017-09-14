/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import android.content.Context;
import android.support.v4.widget.TextViewCompat;
import android.util.AttributeSet;

import org.mozilla.gecko.R;

public class MenuItemIcon extends MenuItemDefault {

    public MenuItemIcon(Context context) {
        this(context, null);
    }

    public MenuItemIcon(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemDefaultStyle);
    }

    public MenuItemIcon(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void refreshIcon() {
        // always show icon, if any
        TextViewCompat.setCompoundDrawablesRelative(this, null, null, super.mIcon, null);
    }
}
