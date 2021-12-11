/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.view.ViewCompat;
import org.mozilla.geckoview.PanZoomController;

public class ToolbarBottomBehavior extends CoordinatorLayout.Behavior<View> {
  public ToolbarBottomBehavior(Context context, AttributeSet attributeSet) {
    super(context, attributeSet);
  }

  @Override
  public boolean onStartNestedScroll(
      CoordinatorLayout coordinatorLayout,
      View child,
      View directTargetChild,
      View target,
      int axes,
      int type) {
    NestedGeckoView geckoView = (NestedGeckoView) target;
    if (axes == ViewCompat.SCROLL_AXIS_VERTICAL
        && geckoView.getInputResult() == PanZoomController.INPUT_RESULT_HANDLED) {
      return true;
    }

    if (geckoView.getInputResult() == PanZoomController.INPUT_RESULT_UNHANDLED) {
      // Restore the toolbar to the original (visible) state, this is what A-C does.
      child.setTranslationY(0f);
    }

    return false;
  }

  @Override
  public void onStopNestedScroll(
      CoordinatorLayout coordinatorLayout, View child, View target, int type) {
    // Snap up or down the user stops scrolling.
    if (child.getTranslationY() >= (child.getHeight() / 2f)) {
      child.setTranslationY(child.getHeight());
    } else {
      child.setTranslationY(0f);
    }
  }

  @Override
  public void onNestedPreScroll(
      CoordinatorLayout coordinatorLayout,
      View child,
      View target,
      int dx,
      int dy,
      int[] consumed,
      int type) {
    super.onNestedPreScroll(coordinatorLayout, child, target, dx, dy, consumed, type);
    child.setTranslationY(Math.max(0f, Math.min(child.getHeight(), child.getTranslationY() + dy)));
  }
}
