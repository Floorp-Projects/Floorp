/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.TextView;

public class AllCapsTextView extends TextView {

    public AllCapsTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        super.setText(text.toString().toUpperCase(), type);
    }
}
