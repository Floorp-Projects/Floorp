/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.BounceAnimator;
import org.mozilla.gecko.animation.BounceAnimator.Attributes;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.support.v4.view.PagerTabStrip;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;

import com.nineoldandroids.animation.AnimatorSet;
import com.nineoldandroids.animation.ObjectAnimator;
import com.nineoldandroids.animation.ValueAnimator;
import com.nineoldandroids.view.ViewHelper;

/**
 * HomePagerTabStrip is a custom implementation of PagerTabStrip
 * that exposes XML attributes for the public methods.
 */

class HomePagerTabStrip extends PagerTabStrip {

    private static final String LOGTAG = "PagerTabStrip";
    private static final int ANIMATION_DELAY_MS = 250;
    private static final int ALPHA_MS = 10;
    private static final int BOUNCE1_MS = 350;
    private static final int BOUNCE2_MS = 200;
    private static final int BOUNCE3_MS = 100;
    private static final int BOUNCE4_MS = 100;
    private static final int INIT_OFFSET = 100;

    private final Paint shadowPaint;
    private final int shadowSize;

    public HomePagerTabStrip(Context context) {
        this(context, null);
    }

    public HomePagerTabStrip(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.HomePagerTabStrip);
        int color = a.getColor(R.styleable.HomePagerTabStrip_tabIndicatorColor, 0x00);
        a.recycle();

        setTabIndicatorColor(color);

        final Resources res = getResources();
        shadowSize = res.getDimensionPixelSize(R.dimen.tabs_strip_shadow_size);

        shadowPaint = new Paint();
        shadowPaint.setColor(res.getColor(R.color.url_bar_shadow));
        shadowPaint.setStrokeWidth(0.0f);

        getViewTreeObserver().addOnPreDrawListener(new PreDrawListener());
    }

    @Override
    public int getPaddingBottom() {
        // PagerTabStrip enforces a minimum bottom padding of 6dp which causes
        // misalignments when using 'center_vertical' gravity. Force padding bottom
        // to 0dp so that children are properly centered.
        return 0;
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        final int height = getHeight();
        canvas.drawRect(0, height - shadowSize, getWidth(), height, shadowPaint);
    }

    private void animateTitles() {
        final View prevTextView = getChildAt(0);
        final View nextTextView = getChildAt(getChildCount() - 1);

        if (prevTextView == null || nextTextView == null) {
            return;
        }

        // Set up initial values for the views that will be animated.
        ViewHelper.setTranslationX(prevTextView, -INIT_OFFSET);
        ViewHelper.setAlpha(prevTextView, 0);
        ViewHelper.setTranslationX(nextTextView, INIT_OFFSET);
        ViewHelper.setAlpha(nextTextView, 0);

        // Alpha animations.
        final ValueAnimator alpha1 = ObjectAnimator.ofFloat(prevTextView, "alpha", 1);
        final ValueAnimator alpha2 = ObjectAnimator.ofFloat(nextTextView, "alpha", 1);

        final AnimatorSet alphaAnimatorSet = new AnimatorSet();
        alphaAnimatorSet.playTogether(alpha1, alpha2);
        alphaAnimatorSet.setStartDelay(ANIMATION_DELAY_MS);
        alphaAnimatorSet.setDuration(ALPHA_MS);

        // Bounce animation.
        final float bounceDistance = getWidth()/100f; // Hack: TextFields still have 0 width here.

        final BounceAnimator prevBounceAnimator = new BounceAnimator(prevTextView, "translationX");
        prevBounceAnimator.queue(new Attributes(bounceDistance, BOUNCE1_MS));
        prevBounceAnimator.queue(new Attributes(-bounceDistance/4, BOUNCE2_MS));
        prevBounceAnimator.queue(new Attributes(0, BOUNCE4_MS));
        prevBounceAnimator.setStartDelay(ANIMATION_DELAY_MS);

        final BounceAnimator nextBounceAnimator = new BounceAnimator(nextTextView, "translationX");
        nextBounceAnimator.queue(new Attributes(-bounceDistance, BOUNCE1_MS));
        nextBounceAnimator.queue(new Attributes(bounceDistance/4, BOUNCE2_MS));
        nextBounceAnimator.queue(new Attributes(0, BOUNCE4_MS));
        nextBounceAnimator.setStartDelay(ANIMATION_DELAY_MS);

        // Start animations.
        alphaAnimatorSet.start();
        prevBounceAnimator.start();
        nextBounceAnimator.start();
    }

    private class PreDrawListener implements ViewTreeObserver.OnPreDrawListener {
        @Override
        public boolean onPreDraw() {
            animateTitles();
            getViewTreeObserver().removeOnPreDrawListener(this);
            return true;
        }
    }
}
