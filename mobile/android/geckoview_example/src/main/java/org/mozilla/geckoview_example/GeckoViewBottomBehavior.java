/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import androidx.appcompat.widget.Toolbar;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import org.mozilla.geckoview.GeckoView;

public class GeckoViewBottomBehavior extends CoordinatorLayout.Behavior<GeckoView> {
  public GeckoViewBottomBehavior(Context context, AttributeSet attributeSet) {
    super(context, attributeSet);
  }

  @Override
  public boolean layoutDependsOn(CoordinatorLayout parent, GeckoView child, View dependency) {
    return dependency instanceof Toolbar;
  }

  @Override
  public boolean onDependentViewChanged(
      CoordinatorLayout parent, GeckoView child, View dependency) {
    child.setVerticalClipping(Math.round(-dependency.getTranslationY()));
    return true;
  }
}
