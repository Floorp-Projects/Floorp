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

@TargetApi(Build.VERSION_CODES.N)
public class DefaultBrowserPreference extends Preference {
    public DefaultBrowserPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public DefaultBrowserPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public boolean shouldBeVisible() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
    }

    @Override
    protected void onClick() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS);
        getContext().startActivity(intent);
    }
}
