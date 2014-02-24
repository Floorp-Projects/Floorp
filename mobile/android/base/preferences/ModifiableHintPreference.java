/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.preference.Preference;
import android.text.Spanned;
import android.text.SpannableStringBuilder;
import android.text.style.ImageSpan;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

class ModifiableHintPreference extends Preference {
    private static final String LOGTAG = "ModifiableHintPref";
    private final Context mContext;

    private final String MATCH_STRING = "%I";
    private final int RESID_TEXT_VIEW = R.id.label_search_hint;
    private final int RESID_DRAWABLE = R.drawable.ab_add_search_engine;
    private final double SCALE_FACTOR = 0.5;

    public ModifiableHintPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    public ModifiableHintPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mContext = context;
    }

    @Override
    protected View onCreateView(ViewGroup parent) {
        View thisView = super.onCreateView(parent);
        configurePreferenceView(thisView);
        return thisView;
    }

    private void configurePreferenceView(View view) {
        TextView textView = (TextView) view.findViewById(RESID_TEXT_VIEW);
        String searchHint = textView.getText().toString();

        // Use an ImageSpan to include the "add search" icon in the Tip.
        int imageSpanIndex = searchHint.indexOf(MATCH_STRING);
        if (imageSpanIndex != -1) {
            // Scale the resource.
            Drawable drawable = mContext.getResources().getDrawable(RESID_DRAWABLE);
            drawable.setBounds(0, 0, (int) (drawable.getIntrinsicWidth() * SCALE_FACTOR),
                               (int) (drawable.getIntrinsicHeight() * SCALE_FACTOR));

            ImageSpan searchIcon = new ImageSpan(drawable);
            final SpannableStringBuilder hintBuilder = new SpannableStringBuilder(searchHint);

            // Insert the image.
            hintBuilder.setSpan(searchIcon, imageSpanIndex, imageSpanIndex + MATCH_STRING.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            textView.setText(hintBuilder, TextView.BufferType.SPANNABLE);
        }
    }
}
