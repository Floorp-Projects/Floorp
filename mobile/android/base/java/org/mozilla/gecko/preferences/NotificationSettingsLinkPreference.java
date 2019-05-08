/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.provider.Settings;
import android.util.AttributeSet;

/**
 * On Android O and following, this will open the app's OS notification settings when clicked on.
 */
public class NotificationSettingsLinkPreference extends Preference {
    public NotificationSettingsLinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }
    public NotificationSettingsLinkPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onClick() {
        final Context context = getContext();
        final Intent intent = getLinkIntent(context);
        context.startActivity(intent);
    }

    @TargetApi(26)
    private Intent getLinkIntent(final Context context) {
        return new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                .putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
    }
}
