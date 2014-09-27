/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;

import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;
import android.util.Log;

@RobocopTarget
public class RestrictedProfiles {
    private static final String LOGTAG = "GeckoRestrictedProfiles";

    // These constants should be in sync with the ones from toolkit/components/parentalcontrols/nsIParentalControlServices.java
    public static enum Restriction {
        DISALLOW_DOWNLOADS(1, "no_download_files"),
        DISALLOW_INSTALL_EXTENSIONS(2, "no_install_extensions"),
        DISALLOW_INSTALL_APPS(3, "no_install_apps"), // UserManager.DISALLOW_INSTALL_APPS
        DISALLOW_BROWSE_FILES(4, "no_browse_files"),
        DISALLOW_SHARE(5, "no_share"),
        DISALLOW_BOOKMARK(6, "no_bookmark"),
        DISALLOW_ADD_CONTACTS(7, "no_add_contacts"),
        DISALLOW_SET_IMAGE(8, "no_set_image"),
        DISALLOW_MODIFY_ACCOUNTS(9, "no_modify_accounts"); // UserManager.DISALLOW_MODIFY_ACCOUNTS

        public final int id;
        public final String name;

        private Restriction(final int id, final String name) {
            this.id = id;
            this.name = name;
        }
    }

    private static String geckoActionToRestriction(int action) {
        for (Restriction rest : Restriction.values()) {
            if (rest.id == action) {
                return rest.name;
            }
        }

        throw new IllegalArgumentException("Unknown action " + action);
    }

    @RobocopTarget
    private static Bundle getRestrictions() {
        final UserManager mgr = (UserManager) GeckoAppShell.getContext().getSystemService(Context.USER_SERVICE);
        return mgr.getUserRestrictions();
    }

    @WrapElementForJNI
    public static boolean isUserRestricted() {
        // Guest mode is supported in all Android versions
        if (GeckoAppShell.getGeckoInterface().getProfile().inGuestMode()) {
            return true;
        }

        if (Versions.preJBMR2) {
            return false;
        }

        return !getRestrictions().isEmpty();
    }

    public static boolean isAllowed(Restriction action) {
        return isAllowed(action.id, null);
    }

    @WrapElementForJNI
    public static boolean isAllowed(int action, String url) {
        // ALl actions are blocked in Guest mode
        if (GeckoAppShell.getGeckoInterface().getProfile().inGuestMode()) {
            return false;
        }

        if (Versions.preJBMR2) {
            return true;
        }

        try {
            // NOTE: Restrictions hold the opposite intention, so we need to flip it
            final String restriction = geckoActionToRestriction(action);
            return !getRestrictions().getBoolean(restriction, false);
        } catch(IllegalArgumentException ex) {
            Log.i(LOGTAG, "Invalid action", ex);
        }

        return true;
    }

    @WrapElementForJNI
    public static String getUserRestrictions() {
        // Guest mode is supported in all Android versions
        if (GeckoAppShell.getGeckoInterface().getProfile().inGuestMode()) {
            StringBuilder builder = new StringBuilder("{ ");

            for (Restriction restriction : Restriction.values()) {
                builder.append("\"" + restriction.name + "\": true, ");
            }

            builder.append(" }");
            return builder.toString();
        }

        if (Versions.preJBMR2) {
            return "{}";
        }

        final JSONObject json = new JSONObject();
        final Bundle restrictions = getRestrictions();
        final Set<String> keys = restrictions.keySet();

        for (String key : keys) {
            try {
                json.put(key, restrictions.get(key));
            } catch (JSONException e) {
            }
        }

        return json.toString();
    }
}
