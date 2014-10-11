/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import org.mozilla.gecko.Tabs;

import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;

class LinkPreference extends Preference {
    private final String mUrl;

    public LinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mUrl = attrs.getAttributeValue(null, "url");
    }
    public LinkPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mUrl = attrs.getAttributeValue(null, "url");
    }

    @Override
    protected void onClick() {
        Tabs.getInstance().loadUrlInTab(mUrl);
        callChangeListener(mUrl);
    }
}
