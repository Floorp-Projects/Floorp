/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.util;

import android.content.res.TypedArray;
import android.os.Build;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

public class ViewUtil {

    /**
     * Enable a circular touch ripple for a given view. This is intended for borderless views,
     * such as (3-dot) menu buttons.
     *
     * Because of platform limitations a square ripple is used on Android 4.
     */
    public static void enableTouchRipple(View view) {
        final TypedArray backgroundDrawableArray;
        if (AppConstants.Versions.feature21Plus) {
            backgroundDrawableArray = view.getContext().obtainStyledAttributes(new int[] { R.attr.selectableItemBackgroundBorderless });
        } else {
            backgroundDrawableArray = view.getContext().obtainStyledAttributes(new int[] { R.attr.selectableItemBackground });
        }

        // This call is deprecated, but the replacement setBackground(Drawable) isn't available
        // until API 16.
        view.setBackgroundDrawable(backgroundDrawableArray.getDrawable(0));
    }

    /**
     * Android framework have a bug margin start/end for RTL between 19~22. We can only use MarginLayoutParamsCompat before 17 and after 23.
     * @param layoutParams
     * @param marginStart
     * @param isLayoutRtl
     */
    public static void setMarginStart(ViewGroup.MarginLayoutParams layoutParams, int marginStart, boolean isLayoutRtl) {
        if (AppConstants.Versions.feature17Plus && AppConstants.Versions.preN) {
            if (isLayoutRtl) {
                layoutParams.rightMargin = marginStart;
            } else {
                layoutParams.leftMargin = marginStart;
            }
        } else {
            MarginLayoutParamsCompat.setMarginStart(layoutParams, marginStart);
        }
    }
}
