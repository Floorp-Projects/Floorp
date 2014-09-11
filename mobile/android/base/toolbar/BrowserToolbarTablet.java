/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

/**
 * A toolbar implementation for tablets.
 */
class BrowserToolbarTablet extends BrowserToolbarTabletBase {

    private static final int FORWARD_ANIMATION_DURATION = 450;

    private final RelativeLayout.LayoutParams urlBarEntryDefaultLayoutParams;
    private final RelativeLayout.LayoutParams urlBarEntryShrunkenLayoutParams;

    private List<View> tabletDisplayModeViews;
    private boolean hidForwardButtonOnStartEditing;

    private final int urlBarViewOffset;
    private final int defaultForwardMargin;

    public BrowserToolbarTablet(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        urlBarEntryDefaultLayoutParams = (RelativeLayout.LayoutParams) urlBarEntry.getLayoutParams();
        // API level 19 adds a RelativeLayout.LayoutParams copy constructor, so we explicitly cast
        // to ViewGroup.MarginLayoutParams to ensure consistency across platforms.
        urlBarEntryShrunkenLayoutParams =
                new RelativeLayout.LayoutParams((ViewGroup.MarginLayoutParams) urlBarEntryDefaultLayoutParams);
        urlBarEntryShrunkenLayoutParams.addRule(RelativeLayout.ALIGN_RIGHT, R.id.edit_layout);
        urlBarEntryShrunkenLayoutParams.addRule(RelativeLayout.ALIGN_LEFT, R.id.edit_layout);
        urlBarEntryShrunkenLayoutParams.leftMargin = 0;

        final Resources res = getResources();
        urlBarViewOffset = res.getDimensionPixelSize(R.dimen.url_bar_offset_left);
        defaultForwardMargin = res.getDimensionPixelSize(R.dimen.forward_default_offset);
    }

    private ArrayList<View> populateTabletViews() {
        final View[] allTabletDisplayModeViews = new View[] {
            actionItemBar,
            backButton,
            menuButton,
            menuIcon,
            tabsButton,
            tabsCounter,
        };
        final ArrayList<View> listToPopulate = new ArrayList<View>(allTabletDisplayModeViews.length);

        // Some tablet devices do not display all of the Views but instead rely on visibility
        // to hide them. Find and return the ones that are relevant to our device.
        for (final View v : allTabletDisplayModeViews) {
            // These views should all be initialized and we explicitly do not
            // check for null because we may be hiding bugs.
            if (v.getVisibility() == View.VISIBLE) {
                listToPopulate.add(v);
            }
        };
        return listToPopulate;
    }

    @Override
    public boolean isAnimating() {
        return false;
    }

    @Override
    protected void triggerStartEditingTransition(final PropertyAnimator animator) {
        if (tabletDisplayModeViews == null) {
            tabletDisplayModeViews = populateTabletViews();
        }

        urlBarEntry.setLayoutParams(urlBarEntryShrunkenLayoutParams);

        // Hide display elements.
        updateChildrenState();
        for (final View v : tabletDisplayModeViews) {
            v.setVisibility(View.INVISIBLE);
        }

        final Tab selectedTab = Tabs.getInstance().getSelectedTab();
        if (selectedTab != null && selectedTab.canDoForward()) {
            hidForwardButtonOnStartEditing = true;
            forwardButton.setVisibility(View.INVISIBLE);
        } else {
            hidForwardButtonOnStartEditing = false;
        }

        // Show editing elements.
        showUrlEditLayout();
    }

    @Override
    protected void triggerStopEditingTransition() {
        if (tabletDisplayModeViews == null) {
            throw new IllegalStateException("We initialize tabletDisplayModeViews in the " +
                    "transition to show editing mode and don't expect stop editing to be called " +
                    "first.");
        }

        urlBarEntry.setLayoutParams(urlBarEntryDefaultLayoutParams);

        // Show display elements.
        updateChildrenState();
        for (final View v : tabletDisplayModeViews) {
            v.setVisibility(View.VISIBLE);
        }

        if (hidForwardButtonOnStartEditing) {
            forwardButton.setVisibility(View.VISIBLE);
        }

        // Hide editing elements.
        hideUrlEditLayout();
    }

    /**
     * Disables all toolbar elements which are not
     * related to editing mode.
     */
    private void updateChildrenState() {
        // Disable toolbar elements while in editing mode
        final boolean enabled = !isEditing();

        if (!enabled) {
            tabsCounter.onEnterEditingMode();
        }

        tabsButton.setEnabled(enabled);
        menuButton.setEnabled(enabled);

        final int actionItemsCount = actionItemBar.getChildCount();
        for (int i = 0; i < actionItemsCount; ++i) {
            actionItemBar.getChildAt(i).setEnabled(enabled);
        }

        final Tab tab = Tabs.getInstance().getSelectedTab();
        if (tab != null) {
            setButtonEnabled(backButton, canDoBack(tab));
            setButtonEnabled(forwardButton, canDoForward(tab));

            // Once the editing mode is finished, we have to ensure that the
            // forward button slides away if necessary. This is because we might
            // have only disabled it (without hiding it) when the toolbar entered
            // editing mode.
            if (!isEditing()) {
                animateForwardButton(canDoForward(tab) ?
                                     ForwardButtonAnimation.SHOW : ForwardButtonAnimation.HIDE);
            }
        }
    }

    @Override
    protected void animateForwardButton(final ForwardButtonAnimation animation) {
        // If the forward button is not visible, we must be
        // in the phone UI.
        if (forwardButton.getVisibility() != View.VISIBLE) {
            return;
        }

        final boolean showing = (animation == ForwardButtonAnimation.SHOW);

        // if the forward button's margin is non-zero, this means it has already
        // been animated to be visible¸ and vice-versa.
        MarginLayoutParams fwdParams = (MarginLayoutParams) forwardButton.getLayoutParams();
        if ((fwdParams.leftMargin > defaultForwardMargin && showing) ||
            (fwdParams.leftMargin == defaultForwardMargin && !showing)) {
            return;
        }

        // We want the forward button to show immediately when switching tabs
        final PropertyAnimator forwardAnim =
                new PropertyAnimator(isSwitchingTabs ? 10 : FORWARD_ANIMATION_DURATION);
        final int width = forwardButton.getWidth() / 2;

        forwardAnim.addPropertyAnimationListener(new PropertyAnimator.PropertyAnimationListener() {
            @Override
            public void onPropertyAnimationStart() {
                if (!showing) {
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
                if (showing) {
                    MarginLayoutParams layoutParams =
                        (MarginLayoutParams) urlDisplayLayout.getLayoutParams();
                    layoutParams.leftMargin = urlBarViewOffset;

                    layoutParams = (MarginLayoutParams) urlEditLayout.getLayoutParams();
                    layoutParams.leftMargin = urlBarViewOffset;
                }

                urlDisplayLayout.finishForwardAnimation();

                MarginLayoutParams layoutParams = (MarginLayoutParams) forwardButton.getLayoutParams();
                layoutParams.leftMargin = defaultForwardMargin + (showing ? width : 0);
                ViewHelper.setTranslationX(forwardButton, 0);

                requestLayout();
            }
        });

        prepareForwardAnimation(forwardAnim, animation, width);
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
                      width);
            anim.attach(forwardButton,
                      PropertyAnimator.Property.ALPHA,
                      1);
        }

        urlDisplayLayout.prepareForwardAnimation(anim, animation, width);
    }
}
