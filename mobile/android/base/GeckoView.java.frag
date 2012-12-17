/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.@VIEWTYPE@;

public class Gecko@VIEWTYPE@ extends @VIEWTYPE@ {
    private static final int[] STATE_PRIVATE_MODE = { R.attr.state_private };
    private static final int[] STATE_LIGHT = { R.attr.state_light };
    private static final int[] STATE_DARK = { R.attr.state_dark };

    private boolean mIsPrivate = false;
    private boolean mIsLight = false;
    private boolean mIsDark = false;

    public Gecko@VIEWTYPE@(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mIsPrivate)
            mergeDrawableStates(drawableState, STATE_PRIVATE_MODE);
        else if (mIsLight)
            mergeDrawableStates(drawableState, STATE_LIGHT);
        else if (mIsDark)
            mergeDrawableStates(drawableState, STATE_DARK);

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

    public void setTheme(boolean isLight) {
        // Set the theme only if it is different from existing theme.
        if ((isLight && mIsLight != isLight) ||
            (!isLight && mIsDark == isLight)) {
            if (isLight) {
                mIsLight = true;
                mIsDark = false;
            } else {
                mIsLight = false;
                mIsDark = true;
            }

            refreshDrawableState();
        }
    }

    public void resetTheme() {
        if (mIsLight || mIsDark) {
            mIsLight = false;
            mIsDark = false;
            refreshDrawableState();
        }
    } 
}
