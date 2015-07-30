/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;

import java.util.Arrays;
import java.util.List;

public class RestrictedProfileConfiguration implements RestrictionConfiguration {
    static List<Restriction> DEFAULT_RESTRICTIONS = Arrays.asList(
            Restriction.DISALLOW_INSTALL_EXTENSION,
            Restriction.DISALLOW_INSTALL_APPS,
            Restriction.DISALLOW_TOOLS_MENU,
            Restriction.DISALLOW_REPORT_SITE_ISSUE,
            Restriction.DISALLOW_IMPORT_SETTINGS,
            Restriction.DISALLOW_DEVELOPER_TOOLS,
            Restriction.DISALLOW_CUSTOMIZE_HOME,
            Restriction.DISALLOW_PRIVATE_BROWSING
    );

    private static final String ABOUT_ADDONS = "about:addons";
    private static final String ABOUT_PRIVATE_BROWSING = "about:privatebrowsing";

    private Context context;

    public RestrictedProfileConfiguration(Context context) {
        this.context = context.getApplicationContext();
    }

    @Override
    public boolean isAllowed(Restriction restriction) {
        return !getAppRestrictions(context).getBoolean(restriction.name, DEFAULT_RESTRICTIONS.contains(restriction));
    }

    @Override
    public boolean canLoadUrl(String url) {
        if (!isAllowed(Restriction.DISALLOW_INSTALL_EXTENSION) && url.toLowerCase().startsWith(ABOUT_ADDONS)) {
            return false;
        }

        if (!isAllowed(Restriction.DISALLOW_PRIVATE_BROWSING) && url.toLowerCase().startsWith(ABOUT_PRIVATE_BROWSING)) {
            return false;
        }

        return true;
    }

    @Override
    public boolean isRestricted() {
        return true;
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private Bundle getAppRestrictions(final Context context) {
        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);
        return mgr.getApplicationRestrictions(context.getPackageName());
    }
}
