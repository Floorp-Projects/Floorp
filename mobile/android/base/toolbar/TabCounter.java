/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.Rotate3DAnimation;
import org.mozilla.gecko.widget.ThemedTextSwitcher;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.AlphaAnimation;
import android.view.animation.AnimationSet;
import android.widget.ViewSwitcher;

public class TabCounter extends ThemedTextSwitcher
                        implements ViewSwitcher.ViewFactory {

    private static final float CENTER_X = 0.5f;
    private static final float CENTER_Y = 1.25f;
    private static final int DURATION = 500;
    private static final float Z_DISTANCE = 200;

    private final AnimationSet mFlipInForward;
    private final AnimationSet mFlipInBackward;
    private final AnimationSet mFlipOutForward;
    private final AnimationSet mFlipOutBackward;
    private final LayoutInflater mInflater;
    private final int mLayoutId;

    private int mCount;

    private enum FadeMode {
        FADE_IN,
        FADE_OUT
    }

    public TabCounter(Context context, AttributeSet attrs) {
        super(context, attrs);

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TabCounter);
        mLayoutId = a.getResourceId(R.styleable.TabCounter_android_layout, R.layout.tabs_counter);
        a.recycle();

        mInflater = LayoutInflater.from(context);

        mFlipInForward = createAnimation(-90, 0, FadeMode.FADE_IN, -1 * Z_DISTANCE, false);
        mFlipInBackward = createAnimation(90, 0, FadeMode.FADE_IN, Z_DISTANCE, false);
        mFlipOutForward = createAnimation(0, -90, FadeMode.FADE_OUT, -1 * Z_DISTANCE, true);
        mFlipOutBackward = createAnimation(0, 90, FadeMode.FADE_OUT, Z_DISTANCE, true);

        removeAllViews();
        setFactory(this);

        if (Versions.feature16Plus) {
            // This adds the TextSwitcher to the a11y node tree, where we in turn
            // could make it return an empty info node. If we don't do this the
            // TextSwitcher's child TextViews get picked up, and we don't want
            // that since the tabs ImageButton is already properly labeled for
            // accessibility.
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            setAccessibilityDelegate(new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {}
                });
        }
    }

    void setCountWithAnimation(int count) {
        // Don't animate from initial state
        if (mCount == 0) {
            setCount(count);
            return;
        }

        if (mCount == count) {
            return;
        }

        if (count < mCount) {
            setInAnimation(mFlipInBackward);
            setOutAnimation(mFlipOutForward);
        } else {
            setInAnimation(mFlipInForward);
            setOutAnimation(mFlipOutBackward);
        }

        // Eliminate screen artifact. Set explicit In/Out animation pair order. This will always
        // animate pair in In->Out child order, prevent alternating use of the Out->In case.
        setDisplayedChild(0);

        // Set In value, trigger animation to Out value
        setCurrentText(String.valueOf(mCount));
        setText(String.valueOf(count));

        mCount = count;
    }

    void setCount(int count) {
        setCurrentText(String.valueOf(count));
        mCount = count;
    }

    // Alpha animations in editing mode cause action bar corruption on the
    // Nexus 7 (bug 961749). As a workaround, skip these animations in editing
    // mode.
    void onEnterEditingMode() {
        final int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            getChildAt(i).clearAnimation();
        }
    }

    private AnimationSet createAnimation(float startAngle, float endAngle,
                                         FadeMode fadeMode,
                                         float zEnd, boolean reverse) {
        final Context context = getContext();
        AnimationSet set = new AnimationSet(context, null);
        set.addAnimation(new Rotate3DAnimation(startAngle, endAngle, CENTER_X, CENTER_Y, zEnd, reverse));
        set.addAnimation(fadeMode == FadeMode.FADE_IN ? new AlphaAnimation(0.0f, 1.0f) :
                                                        new AlphaAnimation(1.0f, 0.0f));
        set.setDuration(DURATION);
        set.setInterpolator(context, android.R.anim.accelerate_interpolator);
        return set;
    }

    @Override
    public View makeView() {
        return mInflater.inflate(mLayoutId, null);
    }

}
