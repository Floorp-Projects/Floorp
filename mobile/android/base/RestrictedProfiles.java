/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;
import org.mozilla.gecko.restrictions.DefaultConfiguration;
import org.mozilla.gecko.restrictions.GuestProfileConfiguration;
import org.mozilla.gecko.restrictions.RestrictedProfileConfiguration;
import org.mozilla.gecko.restrictions.Restriction;
import org.mozilla.gecko.restrictions.RestrictionConfiguration;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;
import android.util.Log;

@RobocopTarget
public class RestrictedProfiles {
    private static final String LOGTAG = "GeckoRestrictedProfiles";

    private static RestrictionConfiguration configuration;

    private static RestrictionConfiguration getConfiguration(Context context) {
        if (configuration == null) {
            configuration = createConfiguration(context);
        }

        return configuration;
    }

    public static synchronized RestrictionConfiguration createConfiguration(Context context) {
        if (configuration != null) {
            // This method is synchronized and another thread might already have created the configuration.
            return configuration;
        }

        if (isGuestProfile()) {
            return new GuestProfileConfiguration();
        } else if(isRestrictedProfile(context)) {
            return new RestrictedProfileConfiguration(context);
        } else {
            return new DefaultConfiguration();
        }
    }

    private static boolean isGuestProfile() {
        return GeckoAppShell.getGeckoInterface().getProfile().inGuestMode();
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static boolean isRestrictedProfile(Context context) {
        if (Versions.preJBMR2) {
            // Early versions don't support restrictions at all
            return false;
        }

        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);
        Bundle restrictions = mgr.getApplicationRestrictions(context.getPackageName());

        for (String key : restrictions.keySet()) {
            if (restrictions.getBoolean(key)) {
                // At least one restriction is enabled -> We are a restricted profile
                return true;
            }
        }

        return false;
    }

    private static Restriction geckoActionToRestriction(int action) {
        for (Restriction rest : Restriction.values()) {
            if (rest.id == action) {
                return rest;
            }
        }

        throw new IllegalArgumentException("Unknown action " + action);
    }

    private static boolean canLoadUrl(final Context context, final String url) {
        return getConfiguration(context).canLoadUrl(url);
    }

    @WrapElementForJNI
    public static boolean isUserRestricted() {
        return isUserRestricted(GeckoAppShell.getContext());
    }

    public static boolean isUserRestricted(final Context context) {
        return getConfiguration(context).isRestricted();
    }

    public static boolean isAllowed(final Context context, final Restriction restriction) {
        return getConfiguration(context).isAllowed(restriction);
    }

    @WrapElementForJNI
    public static boolean isAllowed(int action, String url) {
        final Restriction restriction;
        try {
            restriction = geckoActionToRestriction(action);
        } catch (IllegalArgumentException ex) {
            // Unknown actions represent a coding error, so we
            // refuse the action and log.
            Log.e(LOGTAG, "Unknown action " + action + "; check calling code.");
            return false;
        }

        final Context context = GeckoAppShell.getContext();

        if (Restriction.DISALLOW_BROWSE_FILES == restriction) {
            return canLoadUrl(context, url);
        } else {
            return isAllowed(context, restriction);
        }
    }
}
