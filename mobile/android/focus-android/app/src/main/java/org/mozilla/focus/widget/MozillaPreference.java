/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.widget;

import android.content.Context;
import android.support.v7.preference.Preference;
import android.util.AttributeSet;

import org.mozilla.focus.R;

public class MozillaPreference extends Preference {
    public MozillaPreference(Context context, AttributeSet attrs,
                             int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        final String appName = getContext().getResources().getString(R.string.app_name);
        final String summary = getContext().getResources().getString(R.string.preference_mozilla_summary, appName);

        setSummary(summary);
    }

    public MozillaPreference(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }
}
