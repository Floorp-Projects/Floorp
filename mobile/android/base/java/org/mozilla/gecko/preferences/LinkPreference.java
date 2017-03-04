/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.util.AttributeSet;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Tabs;

class LinkPreference extends Preference {
    private String mUrl;

    public LinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mUrl = attrs.getAttributeValue(null, "url");
    }
    public LinkPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mUrl = attrs.getAttributeValue(null, "url");
    }

    public void setUrl(String url) {
        mUrl = url;
    }

    /**
     * Open Default apps screen of Settings for API Levels>=24. Support URL will open for lower API levels
     */
    @Override
    protected void onClick() {
        if (GeckoPreferences.PREFS_DEFAULT_BROWSER.equals(getKey()) && AppConstants.Versions.feature24Plus) {
            // We are special casing the link to set the default browser here: On old Android versions we
            // link to a SUMO page but on new Android versions we can link to the default app settings where
            // the user can actually set a default browser (Bug 1312686).
            Intent changeDefaultApps = new Intent("android.settings.MANAGE_DEFAULT_APPS_SETTINGS");
            getContext().startActivity(changeDefaultApps);
        } else {
            Tabs.getInstance().loadUrlInTab(mUrl);
            callChangeListener(mUrl);
        }
    }
}
