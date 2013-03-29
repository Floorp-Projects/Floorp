/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.graphics.Rect;
import android.text.SpannableString;
import android.text.style.BackgroundColorSpan;
import android.text.style.UnderlineSpan;
import android.util.AttributeSet;
import android.widget.TextView;

public class LinkTextView extends TextView {
    private final BackgroundColorSpan mFocusBackgroundSpan;
    private boolean mFocusApplied;

    public LinkTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mFocusBackgroundSpan = new BackgroundColorSpan(context.getResources().getColor(R.color.highlight_focused));
    }

    @Override
    public void setText(CharSequence text, BufferType type) {
        SpannableString content = new SpannableString(text + " \u00BB");
        content.setSpan(new UnderlineSpan(), 0, text.length(), 0);

        super.setText(content, BufferType.SPANNABLE);
    }

    @Override
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(focused, direction, previouslyFocusedRect);

        if (focused == mFocusApplied) {
            return;
        }
        mFocusApplied = focused;

        CharSequence text = getText();
        if (text instanceof SpannableString) {
            SpannableString spannable = (SpannableString)text;
            if (focused) {
                spannable.setSpan(mFocusBackgroundSpan, 0, text.length(), 0);
            } else {
                spannable.removeSpan(mFocusBackgroundSpan);
            }
        }
    }
}
