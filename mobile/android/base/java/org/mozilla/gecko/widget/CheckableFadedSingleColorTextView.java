/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Checkable;

public class CheckableFadedSingleColorTextView extends FadedSingleColorTextView implements Checkable {

    private static final int[] STATE_CHECKED = {
        android.R.attr.state_checked
    };

    private boolean checked;

    public CheckableFadedSingleColorTextView(Context context) {
        this(context, null);
    }

    public CheckableFadedSingleColorTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (checked) {
            mergeDrawableStates(drawableState, STATE_CHECKED);
        }

        return drawableState;
    }

    @Override
    public boolean isChecked() {
        return checked;
    }

    @Override
    public void setChecked(boolean checked) {
        if (this.checked == checked) {
            return;
        }

        this.checked = checked;
        refreshDrawableState();
    }

    @Override
    public void toggle() {
        setChecked(!checked);
    }
}
