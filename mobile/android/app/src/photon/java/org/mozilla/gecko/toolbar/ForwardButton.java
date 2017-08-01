/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;

public class ForwardButton extends NavButton {
    public ForwardButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        boolean isLayoutRtl = ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_RTL;
        mBorderPath.reset();
        final float startX;
        if (isLayoutRtl) {
            startX = 0 + mBorderWidth;
        } else {
            startX = width - mBorderWidth;
        }
        mBorderPath.moveTo(startX, 0);
        mBorderPath.lineTo(startX, height);
    }
}
