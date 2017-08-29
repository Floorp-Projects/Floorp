/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.support.design.widget.CoordinatorLayout;
import android.support.design.widget.FloatingActionButton;
import android.util.AttributeSet;
import android.view.View;

public class FloatingEraseButton extends FloatingActionButton {
    public FloatingEraseButton(Context context) {
        super(context);
    }

    public FloatingEraseButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public FloatingEraseButton(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void updateSessionsCount(int tabCount) {
        final CoordinatorLayout.LayoutParams params = (CoordinatorLayout.LayoutParams) getLayoutParams();
        final FloatingActionButtonBehavior behavior = (FloatingActionButtonBehavior) params.getBehavior();

        final boolean shouldBeVisible = tabCount == 1;

        if (behavior != null) {
            behavior.setEnabled(shouldBeVisible);
        }

        if (shouldBeVisible) {
            setVisibility(View.VISIBLE);
        } else {
            setVisibility(View.GONE);
        }
    }
}
