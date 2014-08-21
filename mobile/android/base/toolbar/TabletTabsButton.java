/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.util.AttributeSet;

public class TabletTabsButton extends ShapedButton {
    public TabletTabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        final int curve = (int) (height * 1.125f);

        mPath.reset();

        mPath.moveTo(width, 0);
        mPath.cubicTo((width - (curve * 0.75f)), 0,
                      (width - (curve * 0.25f)), height,
                      (width - curve), height);
        mPath.lineTo(0, height);
        mPath.lineTo(0, 0);
    }
}
