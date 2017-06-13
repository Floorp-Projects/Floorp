package org.mozilla.gecko.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.RelativeLayout;

public class SplashScreen extends RelativeLayout {

    private static long MIN_DISPLAY_TIME = 500;
    private static long MAX_DISPLAY_TIME = 2000;

    private boolean hasReachedThreshold = false;
    private boolean shouldHideAsap = false;

    public SplashScreen(Context context) {
        super(context);
    }

    public SplashScreen(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SplashScreen(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    // check if the splash screen could hide now.
    public void hide() {
        if (getVisibility() == GONE) {
            return;
        }
        if (hasReachedThreshold) {
            vanish();
        } else {
            // if the threshold not reached, mark
            shouldHideAsap = true;
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        // Splash Screen will at least wait for this long before disappear.
        atLeast(MIN_DISPLAY_TIME);
    }

    // the minimum time the splash screen will stay on the screen
    private void atLeast(long millis) {
        postDelayed(new Runnable() {
            @Override
            public void run() {

                // Now the threshold is reached. If you want to hide the splash screen asap, do it now.
                if (shouldHideAsap) {
                    vanish();
                } else {
                    // after the display threshold is met, hasReachedThreshold is set to true.
                    // So others who want to hide the splash screen could use hide() to hide it afterward.
                    hasReachedThreshold = true;
                    // Now the splash screen will continue to wait till MAX_DISPLAY_TIME is reached.
                    atMost(MAX_DISPLAY_TIME - MIN_DISPLAY_TIME);
                }
            }
        }, millis);
    }

    // the maximum time the splash screen will stay on the screen
    private void atMost(long millis) {
        postDelayed(new Runnable() {
            @Override
            public void run() {
                vanish();
            }
        }, millis);
    }

    // tell the splash screen to fade out.
    // Don't bother if it's already animating or gone.
    private void vanish() {
        if (getVisibility() == GONE || getAlpha() < 1) {
            return;
        }
        SplashScreen.this.animate().alpha(0.0f)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        super.onAnimationEnd(animation);
                        SplashScreen.this.setVisibility(GONE);
                    }
                });

    }
}
