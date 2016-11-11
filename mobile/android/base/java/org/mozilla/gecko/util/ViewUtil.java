/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.util;

import android.content.res.TypedArray;
import android.view.View;

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
}
