/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import org.mozilla.gecko.AboutPages;

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
            Restriction.DISALLOW_IMPORT_SETTINGS,
            Restriction.DISALLOW_DEVELOPER_TOOLS,
            Restriction.DISALLOW_CUSTOMIZE_HOME,
            Restriction.DISALLOW_PRIVATE_BROWSING,
            Restriction.DISALLOW_LOCATION_SERVICE,
            Restriction.DISALLOW_DISPLAY_SETTINGS,
            Restriction.DISALLOW_CLEAR_HISTORY,
            Restriction.DISALLOW_MASTER_PASSWORD,
            Restriction.DISALLOW_GUEST_BROWSING,
            Restriction.DISALLOW_DEFAULT_THEME
    );

    private Context context;

    public RestrictedProfileConfiguration(Context context) {
        this.context = context.getApplicationContext();
    }

    @Override
    public boolean isAllowed(Restriction restriction) {
        boolean isAllowed = !getAppRestrictions(context).getBoolean(restriction.name, DEFAULT_RESTRICTIONS.contains(restriction));

        if (isAllowed) {
            // If this restriction is not enforced by the app setup then check wether this is a restriction that is
            // enforced by the system.
            isAllowed = !getUserRestrictions(context).getBoolean(restriction.name, false);
        }

        return isAllowed;
    }

    @Override
    public boolean canLoadUrl(String url) {
        if (!isAllowed(Restriction.DISALLOW_INSTALL_EXTENSION) && AboutPages.isAboutAddons(url)) {
            return false;
        }

        if (!isAllowed(Restriction.DISALLOW_PRIVATE_BROWSING) && AboutPages.isAboutPrivateBrowsing(url)) {
            return false;
        }

        if (AboutPages.isAboutConfig(url)) {
            // Always block access to about:config to prevent circumventing restrictions (Bug 1189233)
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

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private Bundle getUserRestrictions(final Context context) {
        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);
        return mgr.getUserRestrictions();
    }
}
