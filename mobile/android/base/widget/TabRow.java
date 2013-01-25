/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Checkable;
import android.widget.LinearLayout;

public class TabRow extends LinearLayout
                    implements Checkable {
    private static final String LOGTAG = "GeckoTabRow";
    private static final int[] STATE_CHECKED = { android.R.attr.state_checked };
    private boolean mChecked;

    public TabRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        mChecked = false;
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mChecked)
            mergeDrawableStates(drawableState, STATE_CHECKED);

        return drawableState;
    }

    @Override
    public boolean isChecked() {
        return mChecked;
    }

    @Override
    public void setChecked(boolean checked) {
        if (mChecked != checked) {
            mChecked = checked;
            refreshDrawableState();

            int count = getChildCount();
            for (int i=0; i < count; i++) {
                final View child = getChildAt(i);
                if (child instanceof Checkable)
                    ((Checkable) child).setChecked(checked);
            }
        }
    }

    @Override
    public void toggle() {
        mChecked = !mChecked;
    }
}
