/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import android.content.Context;
import android.util.AttributeSet;

import org.mozilla.gecko.widget.themed.ThemedImageButton;

class ToolbarRoundButton extends ThemedImageButton {

    public ToolbarRoundButton(Context context) {
        this(context, null);
    }

    public ToolbarRoundButton(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public ToolbarRoundButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }
}
