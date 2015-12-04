/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout.LayoutParams;

public class Divider extends View {
    public static enum Orientation { HORIZONTAL, VERTICAL };

    // Orientation of the divider.
    private Orientation mOrientation;

    // Density of the device.
    private final int mDensity;

    public Divider(Context context, AttributeSet attrs) {
        super(context, attrs);

        mDensity = (int) context.getResources().getDisplayMetrics().density;

        setOrientation(Orientation.HORIZONTAL);
    }

    public void setOrientation(Orientation orientation) {
        if (mOrientation != orientation) {
            mOrientation = orientation;

            if (mOrientation == Orientation.HORIZONTAL)
                setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, mDensity));
            else
                setLayoutParams(new LayoutParams(mDensity, LayoutParams.MATCH_PARENT));
        }
    }
}
