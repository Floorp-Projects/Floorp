/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.widget.EditText;

/**
 * An EditText subclass that loses focus when the keyboard
 * is dismissed.
 */
public class BackCaptureEditText extends EditText {
    public BackCaptureEditText(Context context) {
        super(context);
    }

    public BackCaptureEditText(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public BackCaptureEditText(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
            clearFocus();
        }
        return super.onKeyPreIme(keyCode, event);
    }
}
