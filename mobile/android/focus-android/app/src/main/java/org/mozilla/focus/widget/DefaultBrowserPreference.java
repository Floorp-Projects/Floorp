/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.preference.Preference;
import android.provider.Settings;
import android.util.AttributeSet;

import org.mozilla.focus.activity.InfoActivity;

@TargetApi(Build.VERSION_CODES.N)
public class DefaultBrowserPreference extends Preference {
    public DefaultBrowserPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public DefaultBrowserPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onClick() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
            getContext().startActivity(intent);
        } else {
            // This link apparently doesn't need the usual SUMO processing: it's not app specific, but platform specific
            final String url = "https://support.mozilla.org/kb/make-firefox-default-browser-android?utm_source=inproduct&amp;utm_medium=settings&amp;utm_campaign=mobileandroid";

            final Intent intent = InfoActivity.getIntentFor(getContext(), url, getTitle().toString());
            getContext().startActivity(intent);
        }
    }
}
