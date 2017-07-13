/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.PropertyAnimator.PropertyAnimationListener;
import org.mozilla.gecko.skin.SkinConfig;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

/**
 * A toolbar implementation for phones.
 */
class BrowserToolbarPhone extends BrowserToolbarPhoneBase {

    private final PropertyAnimationListener showEditingListener;
    private final PropertyAnimationListener stopEditingListener;

    protected boolean isAnimatingEntry;

    protected BrowserToolbarPhone(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        // Create these listeners here, once, to avoid constructing new listeners
        // each time they are set on an animator (i.e. each time the url bar is clicked).
        showEditingListener = new PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { /* Do nothing */ }

            @Override
            public void onPropertyAnimationEnd() {
                isAnimatingEntry = false;
            }
        };

        stopEditingListener = new PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() { /* Do nothing */ }

            @Override
            public void onPropertyAnimationEnd() {
                urlBarTranslatingEdge.setVisibility(View.INVISIBLE);

                final PropertyAnimator buttonsAnimator = new PropertyAnimator(300);
                urlDisplayLayout.prepareStopEditingAnimation(buttonsAnimator);
                buttonsAnimator.start();

                isAnimatingEntry = false;

                // Trigger animation to update the tabs counter once the
                // tabs button is back on screen.
                updateTabCountAndAnimate(Tabs.getInstance().getDisplayCount());
            }
        };
    }

    @Override
    public boolean isAnimating() {
        return isAnimatingEntry;
    }

    @Override
    protected void triggerStartEditingTransition(final PropertyAnimator animator) {
        if (isAnimatingEntry) {
            return;
        }

        // The animation looks cleaner if the text in the URL bar is
        // not selected so clear the selection by clearing focus.
        urlEditLayout.clearFocus();

        urlDisplayLayout.prepareStartEditingAnimation();
        addAnimationsForEditing(animator, true);
        showUrlEditLayout(animator);
        urlBarTranslatingEdge.setVisibility(View.VISIBLE);
        animator.addPropertyAnimationListener(showEditingListener);

        isAnimatingEntry = true; // To be correct, this should be called last.
    }

    @Override
    protected void triggerStopEditingTransition() {
        final PropertyAnimator animator = new PropertyAnimator(250);
        animator.setUseHardwareLayer(false);

        addAnimationsForEditing(animator, false);
        hideUrlEditLayout(animator);
        animator.addPropertyAnimationListener(stopEditingListener);

        isAnimatingEntry = true;
        animator.start();
    }

    private void addAnimationsForEditing(final PropertyAnimator animator, final boolean isEditing) {
        final int curveTranslation;
        final int entryTranslation;
        if (isEditing) {
            curveTranslation = getUrlBarCurveTranslation();
            entryTranslation = getUrlBarEntryTranslation();
        } else {
            curveTranslation = 0;
            entryTranslation = 0;
        }

        // Slide toolbar elements.
        animator.attach(urlBarTranslatingEdge,
                        PropertyAnimator.Property.TRANSLATION_X,
                        entryTranslation);
        animator.attach(tabsButton,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);
        animator.attach(tabsCounter,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);
        animator.attach(menuButton,
                        PropertyAnimator.Property.TRANSLATION_X,
                        curveTranslation);

        // bug 1375351: menuIcon only exists in Australis flavor
        if (SkinConfig.isAustralis()) {
            animator.attach(menuIcon,
                    PropertyAnimator.Property.TRANSLATION_X,
                    curveTranslation);
        }
    }
}
