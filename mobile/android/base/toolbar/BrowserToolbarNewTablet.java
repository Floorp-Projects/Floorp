/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;

import android.content.Context;
import android.util.AttributeSet;

/**
 * A toolbar implementation for the tablet redesign (bug 1014156).
 * Expected to replace BrowserToolbarTablet once complete.
 */
class BrowserToolbarNewTablet extends BrowserToolbarTabletBase {

    private static final int FORWARD_ANIMATION_DURATION = 450;

    private final int forwardButtonTranslationWidth;

    public BrowserToolbarNewTablet(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        forwardButtonTranslationWidth =
                getResources().getDimensionPixelOffset(R.dimen.new_tablet_nav_button_width);

        // The forward button is initially expanded (in the layout file)
        // so translate it for start of the expansion animation; future
        // iterations translate it to this position when hiding and will already be set up.
        ViewHelper.setTranslationX(forwardButton, -forwardButtonTranslationWidth);
    }

    @Override
    public boolean isAnimating() {
        return false;
    }

    @Override
    protected void triggerStartEditingTransition(final PropertyAnimator animator) {
        showUrlEditLayout();
    }

    @Override
    protected void triggerStopEditingTransition() {
        hideUrlEditLayout();
    }

    @Override
    protected void animateForwardButton(final ForwardButtonAnimation animation) {
        final boolean willShowForward = (animation == ForwardButtonAnimation.SHOW);

        // If we're not in the appropriate state to start a particular animation,
        // then we must be in the opposite state and do not need to animate.
        final float forwardOffset = ViewHelper.getTranslationX(forwardButton);
        if ((forwardOffset >= 0 && willShowForward) ||
                forwardOffset < 0 && !willShowForward) {
            return;
        }

        // We want the forward button to show immediately when switching tabs
        final PropertyAnimator forwardAnim =
                new PropertyAnimator(isSwitchingTabs ? 10 : FORWARD_ANIMATION_DURATION);

        forwardAnim.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!willShowForward) {
                    // Set the margin before the transition when hiding the forward button. We
                    // have to do this so that the favicon isn't clipped during the transition
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) urlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = 0;

                    // Do the same on the URL edit container
                    layoutParams = (MarginLayoutParams) urlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = 0;

                    requestLayout();
                    // Note, we already translated the favicon, site security, and text field
                    // in prepareForwardAnimation, so they should appear to have not moved at
                    // all at this point.
                }
            }

            @Override
            public void onPropertyAnimationEnd() {
                if (willShowForward) {
                    // Increase the margins to ensure the text does not run outside the View.
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) urlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = forwardButtonTranslationWidth;

                    layoutParams = (MarginLayoutParams) urlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = forwardButtonTranslationWidth;
                }

                urlDisplayLayout.finishForwardAnimation();

                requestLayout();
            }
        });

        prepareForwardAnimation(forwardAnim, animation, forwardButtonTranslationWidth);
        forwardAnim.start();
    }

    private void prepareForwardAnimation(PropertyAnimator anim, ForwardButtonAnimation animation, int width) {
        if (animation == ForwardButtonAnimation.HIDE) {
            anim.attach(forwardButton,
                      PropertyAnimator.Property.TRANSLATION_X,
                      -width);
            anim.attach(forwardButton,
                      PropertyAnimator.Property.ALPHA,
                      0);

        } else {
            anim.attach(forwardButton,
                      PropertyAnimator.Property.TRANSLATION_X,
                      0);
            anim.attach(forwardButton,
                      PropertyAnimator.Property.ALPHA,
                      1);
        }

        urlDisplayLayout.prepareForwardAnimation(anim, animation, width);
    }

    @Override
    public void triggerTabsPanelTransition(final PropertyAnimator animator, final boolean areTabsShown) {
        // Do nothing.
    }
}
