/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.util;

import android.annotation.TargetApi;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
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
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public static void enableTouchRipple(final View view) {
        final TypedArray backgroundDrawableArray;
        if (AppConstants.Versions.feature21Plus) {
            backgroundDrawableArray = view.getContext().obtainStyledAttributes(new int[] { R.attr.selectableItemBackgroundBorderless });
        } else {
            backgroundDrawableArray = view.getContext().obtainStyledAttributes(new int[] { R.attr.selectableItemBackground });
        }
        final Drawable backgroundDrawable = backgroundDrawableArray.getDrawable(0);
        backgroundDrawableArray.recycle();


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

        // This call is deprecated, but the replacement setBackground(Drawable) isn't available
        // until API 16.
        if (AppConstants.Versions.feature16Plus) {
            view.setBackground(backgroundDrawable);
        } else {
            view.setBackgroundDrawable(backgroundDrawable);
        }

        // Restore the padding on certain devices, as explained above:
        if (!AppConstants.Versions.feature21Plus) {
            view.setPadding(paddingLeft, paddingTop, paddingRight, paddingBottom);
        }
    }

    /**
     * Set text for a TextView, while ensuring it remains centered, regardless of any compound
     * drawables that might be set. When compound drawables exist, text might be shifted left- or
     * right- of centre - this method hacks around that by shifting text back as necessary.
     */
    public static void setCenteredText(final TextView textView, final String label, final int defaultPadding) {
        textView.setText(label);

        // getCompoundDrawables always returns a valid 4 item array (individual drawables may be null,
        // but the array is always non-null).
        final Drawable[] drawables = textView.getCompoundDrawables();
        final Drawable drawableLeft = drawables[0];
        final Drawable drawableRight = drawables[2];

        // The amount by which the left (positive) or right (negative) side has been shifted
        // by drawables:
        final int leftRightDelta = (drawableLeft != null ? drawableLeft.getIntrinsicWidth() : 0)
                - (drawableRight != null ? drawableRight.getIntrinsicWidth() : 0);

        final int leftAdjustment;
        final int rightAdjustment;
        if (leftRightDelta == 0) {
            // No drawables: keep default padding (we still need to make sure we set the padding, since
            // it could have previously been adjusted if there used to be a compound drawable).
            leftAdjustment = 0;
            rightAdjustment = 0;
        } else {
            // We need to decide whether we really want to center: if the text is longer than the available
            // space, we want to keep the maximum available space, if it's shorter we use fake padding
            // to center the text:
            final Rect bounds = new Rect();
            textView.getPaint().getTextBounds(label.toString(), 0, label.length(), bounds);

            // This is the width that would be available _after_ additional padding has been set,
            // i.e. the icon width + mirrored padding + existing paddings:
            final int availableWidth = textView.getWidth() - 2 * Math.abs(leftRightDelta) - 2 * defaultPadding - 2 * textView.getCompoundDrawablePadding();
            final int textWidth = bounds.width();

            if (textWidth > availableWidth) {
                leftAdjustment = 0;
                rightAdjustment = 0;
            } else {
                if (leftRightDelta > 0) {
                    leftAdjustment = 0;
                    rightAdjustment = leftRightDelta;
                } else {
                    leftAdjustment = -leftRightDelta;
                    rightAdjustment = 0;
                }
            }
        }

        textView.setPadding(defaultPadding + leftAdjustment, textView.getPaddingTop(), defaultPadding + rightAdjustment, textView.getPaddingBottom());
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
     * RTL compatibility wrapper to set TextDirection
     * @param textView
     * @param textDirection
     */
    public static void setTextDirection(TextView textView, int textDirection) {
        if (AppConstants.Versions.feature17Plus) {
            textView.setTextDirection(textDirection);
        }
    }

    /**
     * RTL compatibility wrapper to force set TextDirection as RTL for JB mr1 and above
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

    /**
     * Get if the resolved layout direction is <em>right to left</em> or <em>left to right</em>.
     * @param view View to get layout direction for
     * @return <code>true</code> if the horizontal layout direction of this view is from <em>right to left</em><br>
     *     <code>false</code> if the horizontal layout direction of this view is from <em>left to right</em>
     */
    public static boolean isLayoutRtl(final View view) {
        return ViewCompat.getLayoutDirection(view) == ViewCompat.LAYOUT_DIRECTION_RTL;
    }
}
