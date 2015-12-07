/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.animation;

import android.view.View;
import android.view.animation.Animation;
import android.view.animation.Transformation;

public class HeightChangeAnimation extends Animation {
    int mFromHeight;
    int mToHeight;
    View mView;

    public HeightChangeAnimation(View view, int fromHeight, int toHeight) {
        mView = view;
        mFromHeight = fromHeight;
        mToHeight = toHeight;
    }

    @Override
    protected void applyTransformation(float interpolatedTime, Transformation t) {
        mView.getLayoutParams().height = Math.round((mFromHeight * (1 - interpolatedTime)) + (mToHeight * interpolatedTime));
        mView.requestLayout();
    }
}
