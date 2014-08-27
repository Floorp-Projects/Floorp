/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.animation.PropertyAnimator;

import android.content.Context;
import android.util.AttributeSet;

/**
 * A toolbar implementation for the tablet redesign (bug 1014156).
 * Expected to replace BrowserToolbarTablet once complete.
 */
class BrowserToolbarNewTablet extends BrowserToolbarTabletBase {

    public BrowserToolbarNewTablet(final Context context, final AttributeSet attrs) {
        super(context, attrs);
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
        // TODO: Implement.
    }
}
