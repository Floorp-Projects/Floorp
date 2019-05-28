/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityManager;
import android.view.animation.AnimationUtils;

import org.mozilla.focus.R;

public class FloatingEraseButton extends FloatingActionButton {
    private boolean keepHidden;

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
        AccessibilityManager accessibilityManager = (AccessibilityManager) getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);

        keepHidden = tabCount != 1;

        if (behavior != null) {
            if (accessibilityManager != null && accessibilityManager.isTouchExplorationEnabled()) {
                // Always display erase button if Talk Back is enabled
                behavior.setEnabled(false);
            } else {
                behavior.setEnabled(!keepHidden);
            }
        }

        if (keepHidden) {
            setVisibility(View.GONE);
        }
    }

    @Override
    protected void onFinishInflate() {
        if (!keepHidden) {
            this.setVisibility(View.VISIBLE);
            this.startAnimation(AnimationUtils.loadAnimation(getContext(), R.anim.fab_reveal));
        }

        super.onFinishInflate();
    }

    @Override
    public void setVisibility(int visibility) {
        if (keepHidden && visibility == View.VISIBLE) {
            // There are multiple callbacks updating the visibility of the button. Let's make sure
            // we do not show the button if we do not want to.
            return;
        }

        if (visibility == View.VISIBLE) {
            show();
        } else {
            hide();
        }
    }
}
