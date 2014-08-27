/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.animation.PropertyAnimator;

import android.content.Context;
import android.util.AttributeSet;

/**
 * A toolbar implementation for tablets.
 */
class BrowserToolbarTablet extends BrowserToolbarTabletBase {

    public BrowserToolbarTablet(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        // TODO: Implement.
    }

    @Override
    public boolean isAnimating() {
        return false;
    }

    @Override
    protected void triggerStartEditingTransition(final PropertyAnimator animator) {
        // TODO: Implement.
    }

    @Override
    protected void triggerStopEditingTransition() {
        // TODO: Implement.
    }
}
