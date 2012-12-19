/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.@VIEWTYPE@;

public class Gecko@VIEWTYPE@ extends @VIEWTYPE@ {
    private static final int[] STATE_PRIVATE_MODE = { R.attr.state_private };

    private boolean mIsPrivate = false;

    public Gecko@VIEWTYPE@(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mIsPrivate)
            mergeDrawableStates(drawableState, STATE_PRIVATE_MODE);

        return drawableState;
    }

    public boolean isPrivateMode() {
        return mIsPrivate;
    }

    public void setPrivateMode(boolean isPrivate) {
        if (mIsPrivate != isPrivate) {
            mIsPrivate = isPrivate;
            refreshDrawableState();
        }
   }
}
