/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

/**
 * The toolbar implementation for tablet.
 */
class BrowserToolbarTablet extends BrowserToolbarTabletBase {

    private static final int FORWARD_ANIMATION_DURATION = 450;

    private enum ForwardButtonState {
        HIDDEN,
        DISPLAYED,
        TRANSITIONING,
    }

    private final int forwardButtonTranslationWidth;

    private ForwardButtonState forwardButtonState;

    private boolean backButtonWasEnabledOnStartEditing;

    public BrowserToolbarTablet(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        forwardButtonTranslationWidth =
                getResources().getDimensionPixelOffset(R.dimen.new_tablet_nav_button_width);

        // The forward button is initially expanded (in the layout file)
        // so translate it for start of the expansion animation; future
        // iterations translate it to this position when hiding and will already be set up.
        ViewHelper.setTranslationX(forwardButton, -forwardButtonTranslationWidth);

        // TODO: Move this to *TabletBase when old tablet is removed.
        // We don't want users clicking the forward button in transitions, but we don't want it to
        // look disabled to avoid flickering complications (e.g. disabled in editing mode), so undo
        // the work of the super class' constructor.
        setButtonEnabled(forwardButton, true);

        updateForwardButtonState(ForwardButtonState.HIDDEN);
    }

    private void updateForwardButtonState(final ForwardButtonState state) {
        forwardButtonState = state;
        forwardButton.setEnabled(forwardButtonState == ForwardButtonState.DISPLAYED);
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
        if ((forwardButtonState != ForwardButtonState.HIDDEN && willShowForward) ||
                (forwardButtonState != ForwardButtonState.DISPLAYED && !willShowForward)) {
            return;
        }
        updateForwardButtonState(ForwardButtonState.TRANSITIONING);

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
                final ForwardButtonState newForwardButtonState;
                if (willShowForward) {
                    // Increase the margins to ensure the text does not run outside the View.
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) urlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = forwardButtonTranslationWidth;

                    layoutParams = (MarginLayoutParams) urlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = forwardButtonTranslationWidth;

                    newForwardButtonState = ForwardButtonState.DISPLAYED;
                } else {
                    newForwardButtonState = ForwardButtonState.HIDDEN;
                }

                urlDisplayLayout.finishForwardAnimation();
                updateForwardButtonState(newForwardButtonState);

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

    @Override
    public void setToolBarButtonsAlpha(float alpha) {
        // Do nothing.
    }


    @Override
    public void startEditing(final String url, final PropertyAnimator animator) {
        // We already know the forward button state - no need to store it here.
        backButtonWasEnabledOnStartEditing = backButton.isEnabled();

        setButtonEnabled(backButton, false);
        setButtonEnabled(forwardButton, false);

        super.startEditing(url, animator);
    }

    @Override
    public String commitEdit() {
        stopEditingNewTablet();
        return super.commitEdit();
    }

    @Override
    public String cancelEdit() {
        // This can get called when we're not editing but we only want
        // to make these changes when leaving editing mode.
        if (isEditing()) {
            stopEditingNewTablet();

            setButtonEnabled(backButton, backButtonWasEnabledOnStartEditing);
            updateForwardButtonState(forwardButtonState);
        }

        return super.cancelEdit();
    }

    private void stopEditingNewTablet() {
        // Undo the changes caused by calling setButtonEnabled in startEditing.
        // Note that this should be called first so the enabled state of the
        // forward button is set to the proper value.
        setButtonEnabled(forwardButton, true);
    }

    @Override
    protected Drawable getLWTDefaultStateSetDrawable() {
        return BrowserToolbar.getLightweightThemeDrawable(this, getResources(), getTheme(),
                R.color.background_normal);
    }
}
