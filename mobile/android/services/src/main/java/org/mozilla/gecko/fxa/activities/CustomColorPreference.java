/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import org.mozilla.gecko.R;
import android.content.Context;
import android.content.res.TypedArray;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

 /**
  * This preference is used to define custom  colors for both title and summary texts.
  * Color code #777777 (placeholder_grey) is used as the fallback color for both title and summary.
  */
public class CustomColorPreference extends Preference {
    private int mTitleColor;
    private int mSummaryColor;

    public CustomColorPreference(Context context) {
        super(context);
    }

    public CustomColorPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public CustomColorPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(context, attrs);
    }

    public void init(Context context, AttributeSet attrs) {
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.CustomColorPreference);
        mTitleColor = a.getColor(R.styleable.CustomColorPreference_titleColor, R.color.placeholder_grey);
        mSummaryColor = a.getColor(R.styleable.CustomColorPreference_summaryColor, R.color.placeholder_grey);
        a.recycle();
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);
        final TextView title = (TextView) view.findViewById(android.R.id.title);
        final TextView summary = (TextView) view.findViewById(android.R.id.summary);
        title.setTextColor(mTitleColor);
        summary.setTextColor(mSummaryColor);
    }
}
