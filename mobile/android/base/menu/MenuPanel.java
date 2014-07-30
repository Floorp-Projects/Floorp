/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;

import android.content.Context;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.LinearLayout;

/**
 * The outer container for the custom menu. On phones with h/w menu button,
 * this is given to Android which inflates it to the right panel. On phones 
 * with s/w menu button, this is added to a MenuPopup.
 */
public class MenuPanel extends LinearLayout {
    public MenuPanel(Context context, AttributeSet attrs) {
        super(context, attrs);

        int width = (int) context.getResources().getDimension(R.dimen.menu_item_row_width);
        setLayoutParams(new ViewGroup.LayoutParams(width, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Restrict the height to 75% of the screen-height. heightPixels changes during rotation.
        DisplayMetrics metrics = getContext().getResources().getDisplayMetrics();
        int restrictedHeightSpec = MeasureSpec.makeMeasureSpec((int) (0.75 * metrics.heightPixels), MeasureSpec.AT_MOST);

        super.onMeasure(widthMeasureSpec, restrictedHeightSpec);
    }

    @Override
    public boolean dispatchPopulateAccessibilityEvent (AccessibilityEvent event) {
        if (Versions.feature14Plus) {
            onPopulateAccessibilityEvent(event);
        }

        return true;
    }
}
