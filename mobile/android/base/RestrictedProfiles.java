/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;

import java.lang.StringBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.HashSet;

import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.UserManager;
import android.util.Log;

@RobocopTarget
public class RestrictedProfiles {
    private static final String LOGTAG = "GeckoRestrictedProfiles";

    private static Boolean inGuest = null;

    @SuppressWarnings("serial")
    private static final List<String> BANNED_SCHEMES = new ArrayList<String>() {{
        add("file");
        add("chrome");
        add("resource");
        add("jar");
        add("wyciwyg");
    }};

    private static boolean getInGuest() {
        if (inGuest == null) {
            inGuest = GeckoAppShell.getGeckoInterface().getProfile().inGuestMode();
        }

        return inGuest;
    }

    @SuppressWarnings("serial")
    private static final List<String> BANNED_URLS = new ArrayList<String>() {{
        add("about:config");
    }};

    /* This is a list of things we can restrict you from doing. Some of these are reflected in Android UserManager constants.
     * Others are specific to us.
     * These constants should be in sync with the ones from toolkit/components/parentalcontrols/nsIParentalControlServices.idl
     */
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

    private static Restriction geckoActionToRestriction(int action) {
        for (Restriction rest : Restriction.values()) {
            if (rest.id == action) {
                return rest;
            }
        }

        throw new IllegalArgumentException("Unknown action " + action);
    }

    @RobocopTarget
    private static Bundle getRestrictions() {
        final UserManager mgr = (UserManager) GeckoAppShell.getContext().getSystemService(Context.USER_SERVICE);
        return mgr.getUserRestrictions();
    }

    private static boolean canLoadUrl(final String url) {
        // Null urls are always allowed
        if (url == null) {
            return true;
        }

        try {
            // If we're not in guest mode, and the system restriction isn't in place, everything is allowed.
            if (!getInGuest() &&
                !getRestrictions().getBoolean(Restriction.DISALLOW_BROWSE_FILES.name, false)) {
                return true;
            }
        } catch(IllegalArgumentException ex) {
            Log.i(LOGTAG, "Invalid action", ex);
        }

        final Uri u = Uri.parse(url);
        final String scheme = u.getScheme();
        if (BANNED_SCHEMES.contains(scheme)) {
            return false;
        }

        for (String banned : BANNED_URLS) {
            if (url.startsWith(banned)) {
                return false;
            }
        }

        // TODO: The UserManager should support blacklisting urls by the device owner.
        return true;
    }

    @WrapElementForJNI
    public static boolean isUserRestricted() {
        // Guest mode is supported in all Android versions
        if (getInGuest()) {
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
        final Restriction restriction;
        try {
            restriction = geckoActionToRestriction(action);
        } catch(IllegalArgumentException ex) {
            return true;
        }

        if (Restriction.DISALLOW_BROWSE_FILES == restriction) {
            return canLoadUrl(url);
        }

        // ALl actions are blocked in Guest mode
        if (getInGuest()) {
            return false;
        }

        if (Versions.preJBMR2) {
            return true;
        }

        try {
            // NOTE: Restrictions hold the opposite intention, so we need to flip it
            return !getRestrictions().getBoolean(restriction.name, false);
        } catch(IllegalArgumentException ex) {
            Log.i(LOGTAG, "Invalid action", ex);
        }

        return true;
    }

    @WrapElementForJNI
    public static String getUserRestrictions() {
        // Guest mode is supported in all Android versions
        if (getInGuest()) {
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
