/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.util.AttributeSet;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.tabs.TabCurve;

public class PhoneTabsButton extends ShapedButton {
    public PhoneTabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        if (!AppConstants.Versions.preLollipop) {
            // The Android N preview has issues rendering our tabs button during animations
            // unless we use hardware layers, see bug 1264783. For consistency we should
            // try and set this across all devices, however on 4.X devices the background
            // isn't drawn when we use hardware layers - hence we need to disable this there.
            setLayerType(LAYER_TYPE_HARDWARE, null);
        }
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();

        mPath.moveTo(0, 0);
        TabCurve.drawFromTop(mPath, 0, height, TabCurve.Direction.RIGHT);
        mPath.lineTo(width, height);
        mPath.lineTo(width, 0);
        mPath.lineTo(0, 0);
    }
}
