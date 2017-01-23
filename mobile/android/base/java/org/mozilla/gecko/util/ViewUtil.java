/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.util;

import android.annotation.TargetApi;
import android.content.res.TypedArray;
import android.os.Build;
import android.support.v4.text.TextUtilsCompat;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;

import java.util.Locale;

public class ViewUtil {

    /**
     * Enable a circular touch ripple for a given view. This is intended for borderless views,
     * such as (3-dot) menu buttons.
     *
     * Because of platform limitations a square ripple is used on Android 4.
     */
    public static void enableTouchRipple(final View view) {
        // On certain older devices (e.g. ASUS TF200T, Motorola Droid 4), setting a background
        // drawable results in the padding getting wiped. We work around this by saving the pre-existing
        // padding, followed by restoring it at the end in view.setPadding().

        // Unfortunately the IDE and compiler aren't clever enough for us to be able to make
        // these final (and uninitialised). So we just use garbage values instead:
        int paddingLeft = -1;
        int paddingTop = -1;
        int paddingRight = -1;
        int paddingBottom = -1;

        if (!AppConstants.Versions.feature21Plus) {
            paddingLeft = view.getPaddingLeft();
            paddingTop = view.getPaddingTop();
            paddingRight = view.getPaddingRight();
            paddingBottom = view.getPaddingBottom();
        }

        setTouchRipple(view);

        if (!AppConstants.Versions.feature21Plus) {
            view.setPadding(paddingLeft, paddingTop, paddingRight, paddingBottom);
        }
    }

    private static void setTouchRipple(final View view) {
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

    /**
     * Force set layout direction to RTL or LTR by Locale.
     * @param view
     * @param locale
     */
    public static void setLayoutDirection(View view, Locale locale) {
        switch (TextUtilsCompat.getLayoutDirectionFromLocale(locale)) {
            case ViewCompat.LAYOUT_DIRECTION_RTL:
                ViewCompat.setLayoutDirection(view, ViewCompat.LAYOUT_DIRECTION_RTL);
                break;
            case ViewCompat.LAYOUT_DIRECTION_LTR:
            default:
                ViewCompat.setLayoutDirection(view, ViewCompat.LAYOUT_DIRECTION_LTR);
                break;
        }
    }

    /**
     * RTL compatibility wrapper to force set TextDirection for JB mr1 and above
     *
     * @param textView
     * @param isRtl
     */
    public static void setTextDirectionRtlCompat(TextView textView, boolean isRtl) {
        if (AppConstants.Versions.feature17Plus) {
            setTextDirectionRtlCompat17(textView, isRtl);
        }
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR1)
    private static void setTextDirectionRtlCompat17(TextView textView, boolean isRtl) {
        if (isRtl) {
            textView.setTextDirection(View.TEXT_DIRECTION_RTL);
        } else {
            textView.setTextDirection(View.TEXT_DIRECTION_LTR);
        }
    }
}
