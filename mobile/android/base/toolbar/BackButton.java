/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.graphics.Path;
import android.util.AttributeSet;

public class BackButton extends NavButton {
    public BackButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    // TODO: The only reason we can't inherit this method is because there's a color inconsistency.
    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);
        mBorderPaint.setColor(isPrivate ? 0xFF363B40 : 0xFFB5B5B5);
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);

        mPath.reset();
        mPath.addCircle(width/2, height/2, width/2, Path.Direction.CW);

        mBorderPath.reset();
        mBorderPath.addCircle(width/2, height/2, (width/2) - (mBorderWidth/2), Path.Direction.CW);
    }
}
