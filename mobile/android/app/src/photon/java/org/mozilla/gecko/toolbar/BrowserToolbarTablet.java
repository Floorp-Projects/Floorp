/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.R;
import org.mozilla.gecko.animation.PropertyAnimator;
import org.mozilla.gecko.util.ViewUtil;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;

/**
 * The toolbar implementation for tablet.
 */
class BrowserToolbarTablet extends BrowserToolbarTabletBase {

    private boolean backButtonWasEnabledOnStartEditing;
    private boolean forwardButtonWasEnabledOnStartEditing;

    public BrowserToolbarTablet(final Context context, final AttributeSet attrs) {
        super(context, attrs);

        forwardButton.setEnabled(false);
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
    public void triggerTabsPanelTransition(final PropertyAnimator animator, final boolean areTabsShown) {
        // Do nothing.
    }

    @Override
    public void startEditing(final String url, final PropertyAnimator animator) {
        // We already know the forward button state - no need to store it here.
        backButtonWasEnabledOnStartEditing = backButton.isEnabled();
        forwardButtonWasEnabledOnStartEditing = forwardButton.isEnabled();

        backButton.setEnabled(false);
        forwardButton.setEnabled(false);

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

            backButton.setEnabled(backButtonWasEnabledOnStartEditing);
            forwardButton.setEnabled(forwardButtonWasEnabledOnStartEditing);
        }

        return super.cancelEdit();
    }

    private void stopEditingNewTablet() {
        // Undo the changes caused by calling setEnabled for forwardButton in startEditing.
        // Note that this should be called first so the enabled state of the
        // forward button is set to the proper value.
        forwardButton.setEnabled(true);
    }

    @Override
    protected Drawable getLWTDefaultStateSetDrawable() {
        return BrowserToolbar.getLightweightThemeDrawable(this, getTheme(), R.color.toolbar_grey);
    }
}
