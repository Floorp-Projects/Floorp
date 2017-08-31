/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.TypedArray;
import android.support.annotation.Nullable;
import android.support.v7.widget.SwitchCompat;
import android.util.AttributeSet;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;

/**
 * A toggle that controls a SharedPreference preference, and can be added outside of PreferenceScreen.
 *
 * The attribute 'androidPreferenceKey' must be defined when using this layout.
 */
public class SwitchPreferenceView extends SwitchCompat {

    public SwitchPreferenceView(Context context) {
        super(context);
    }

    public SwitchPreferenceView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public SwitchPreferenceView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    private void init(Context context, AttributeSet attrs) {
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.SwitchPreferenceView);
        final String preferenceKey = a.getString(R.styleable.SwitchPreferenceView_androidPreferenceKey);
        final boolean defaultValue = a.getBoolean(R.styleable.SwitchPreferenceView_defaultValue, false);

        if (preferenceKey == null ) {
            throw new IllegalStateException("The 'androidPreferenceKey' attribute must be included in the layout");
        }

        a.recycle();

        final SharedPreferences sharedPreferences = GeckoSharedPrefs.forProfile(context);
        final boolean isChecked = sharedPreferences.getBoolean(preferenceKey, defaultValue);

        setChecked(isChecked);
        setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
                sharedPreferences.edit().putBoolean(preferenceKey, b).apply();
            }
        });

    }
}
