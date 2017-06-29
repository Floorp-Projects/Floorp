/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.Context;
import android.util.AttributeSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.widget.themed.ThemedLinearLayout;
import org.mozilla.gecko.widget.themed.ThemedTextView;

public class SuggestionItem extends ThemedLinearLayout {

    private ThemedTextView suggestionText;

    public SuggestionItem(Context context) {
        this(context, null);
    }

    public SuggestionItem(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        suggestionText = (ThemedTextView) findViewById(R.id.suggestion_text);
    }

    @Override
    public void setPrivateMode(boolean isPrivate) {
        super.setPrivateMode(isPrivate);

        if (suggestionText != null) {
            suggestionText.setPrivateMode(isPrivate);
        }
    }
}

