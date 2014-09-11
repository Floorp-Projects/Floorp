/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.animation.ViewHelper;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.util.AttributeSet;

/**
 * A toolbar implementation for pre-Honeycomb devices.
 * The toolbar changes editing mode state without animating.
 */
class BrowserToolbarPreHC extends BrowserToolbarPhoneBase {

    public BrowserToolbarPreHC(final Context context, final AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean isAnimating() {
        return false;
    }

    @Override
    protected void triggerStartEditingTransition(final PropertyAnimator animator) {
        showUrlEditLayout();
        updateEditingState(true);
    }

    @Override
    protected void triggerStopEditingTransition() {
        hideUrlEditLayout();
        updateTabCountAndAnimate(Tabs.getInstance().getDisplayCount());
        updateEditingState(false);
    }

    private void updateEditingState(final boolean isEditing) {
        final int curveTranslation;
        final int entryTranslation;
        if (isEditing) {
            curveTranslation = getUrlBarCurveTranslation();
            entryTranslation = getUrlBarEntryTranslation();
        } else {
            curveTranslation = 0;
            entryTranslation = 0;
        }

        // Prevent taps through the editing mode cancel button (bug 1001243).
        tabsButton.setEnabled(!isEditing);

        ViewHelper.setTranslationX(urlBarTranslatingEdge, entryTranslation);
        ViewHelper.setTranslationX(tabsButton, curveTranslation);
        ViewHelper.setTranslationX(tabsCounter, curveTranslation);

        if (!HardwareUtils.hasMenuButton()) {
            // Prevent tabs through the editing mode cancel button (bug 1001243).
            menuButton.setEnabled(!isEditing);

            ViewHelper.setTranslationX(menuButton, curveTranslation);
            ViewHelper.setTranslationX(menuIcon, curveTranslation);
        }
    }
}
