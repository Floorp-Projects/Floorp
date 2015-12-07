/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.CheckBox;
import android.widget.Checkable;
import android.widget.LinearLayout;


public class CheckableLinearLayout extends LinearLayout implements Checkable {

    private CheckBox mCheckBox;

    public CheckableLinearLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean isChecked() {
        return mCheckBox != null && mCheckBox.isChecked();
    }

    @Override
    public void setChecked(boolean isChecked) {
        if (mCheckBox != null) {
            mCheckBox.setChecked(isChecked);
        }
    }

    @Override
    public void toggle() {
        if (mCheckBox != null) {
            mCheckBox.toggle();
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mCheckBox = (CheckBox) findViewById(R.id.checkbox);
        mCheckBox.setClickable(false);
    }
}


