/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.util.AttributeSet;

public class ForwardButton extends NavButton {
    public ForwardButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mBorderPath.reset();
        mBorderPath.moveTo(width - mBorderWidth, 0);
        mBorderPath.lineTo(width - mBorderWidth, height);
    }
}
