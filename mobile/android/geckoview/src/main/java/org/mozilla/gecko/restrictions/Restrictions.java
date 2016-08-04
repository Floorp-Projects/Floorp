/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.util.Log;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.restrictions.DefaultConfiguration;
import org.mozilla.gecko.restrictions.GuestProfileConfiguration;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.restrictions.RestrictedProfileConfiguration;
import org.mozilla.gecko.restrictions.RestrictionCache;
import org.mozilla.gecko.restrictions.RestrictionConfiguration;

@RobocopTarget
public class Restrictions {
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

        if (isGuestProfile(context)) {
            return new GuestProfileConfiguration();
        } else if (isRestrictedProfile(context)) {
            return new RestrictedProfileConfiguration(context);
        } else {
            return new DefaultConfiguration();
        }
    }

    private static boolean isGuestProfile(Context context) {
        if (configuration != null) {
            return configuration instanceof GuestProfileConfiguration;
        }

        GeckoAppShell.GeckoInterface geckoInterface = GeckoAppShell.getGeckoInterface();
        if (geckoInterface != null) {
            return geckoInterface.getProfile().inGuestMode();
        }

        return GeckoProfile.get(context).inGuestMode();
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    public static boolean isRestrictedProfile(Context context) {
        if (configuration != null) {
            return configuration instanceof RestrictedProfileConfiguration;
        }

        if (Versions.preJBMR2) {
            // Early versions don't support restrictions at all
            return false;
        }

        // The user is on a restricted profile if, and only if, we injected application restrictions during account setup.
        return RestrictionCache.hasApplicationRestrictions(context);
    }

    public static void update(Context context) {
        getConfiguration(context).update();
    }

    private static Restrictable geckoActionToRestriction(int action) {
        for (Restrictable rest : Restrictable.values()) {
            if (rest.id == action) {
                return rest;
            }
        }

        throw new IllegalArgumentException("Unknown action " + action);
    }

    private static boolean canLoadUrl(final Context context, final String url) {
        return getConfiguration(context).canLoadUrl(url);
    }

    @WrapForJNI
    public static boolean isUserRestricted() {
        return isUserRestricted(GeckoAppShell.getApplicationContext());
    }

    public static boolean isUserRestricted(final Context context) {
        return getConfiguration(context).isRestricted();
    }

    public static boolean isAllowed(final Context context, final Restrictable restrictable) {
        return getConfiguration(context).isAllowed(restrictable);
    }

    @WrapForJNI
    public static boolean isAllowed(int action, String url) {
        final Restrictable restrictable;
        try {
            restrictable = geckoActionToRestriction(action);
        } catch (IllegalArgumentException ex) {
            // Unknown actions represent a coding error, so we
            // refuse the action and log.
            Log.e(LOGTAG, "Unknown action " + action + "; check calling code.");
            return false;
        }

        final Context context = GeckoAppShell.getApplicationContext();

        if (Restrictable.BROWSE == restrictable) {
            return canLoadUrl(context, url);
        } else {
            return isAllowed(context, restrictable);
        }
    }
}
