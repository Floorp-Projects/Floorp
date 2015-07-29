/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;
import android.annotation.TargetApi;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.UserManager;
import android.util.Log;

@RobocopTarget
public class RestrictedProfiles {
    private static final String LOGTAG = "GeckoRestrictedProfiles";

    private static volatile Boolean inGuest = null;

    @SuppressWarnings("serial")
    private static final List<String> BANNED_SCHEMES = new ArrayList<String>() {{
        add("file");
        add("chrome");
        add("resource");
        add("jar");
        add("wyciwyg");
    }};

    /**
     * This is a hack to allow non-GeckoApp activities to safely call into
     * RestrictedProfiles without reworking this class or GeckoProfile.
     *
     * It can be removed after Bug 1077590 lands.
     */
    public static void initWithProfile(GeckoProfile profile) {
        inGuest = profile.inGuestMode();
    }

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
    public enum Restriction {
        // These restrictions have no strings assigned because they are only used in guest mode and not shown in the
        // restricted profiles settings UI
        DISALLOW_DOWNLOADS(1, "no_download_files", 0, 0),
        DISALLOW_BROWSE_FILES(4, "no_browse_files", 0, 0),
        DISALLOW_SHARE(5, "no_share", 0, 0),
        DISALLOW_BOOKMARK(6, "no_bookmark", 0, 0),
        DISALLOW_ADD_CONTACTS(7, "no_add_contacts", 0, 0),
        DISALLOW_SET_IMAGE(8, "no_set_image", 0, 0),
        DISALLOW_MODIFY_ACCOUNTS(9, "no_modify_accounts", 0, 0), // UserManager.DISALLOW_MODIFY_ACCOUNTS
        DISALLOW_REMOTE_DEBUGGING(10, "no_remote_debugging", 0, 0),

        // These restrictions are used for restricted profiles and therefore need to have strings assigned for the profile
        // settings UI.
        DISALLOW_INSTALL_EXTENSION(2, "no_install_extensions", R.string.restriction_disallow_addons_title, R.string.restriction_disallow_addons_description),
        DISALLOW_INSTALL_APPS(3, "no_install_apps", R.string.restriction_disallow_apps_title, R.string.restriction_disallow_apps_description), // UserManager.DISALLOW_INSTALL_APPS
        DISALLOW_IMPORT_SETTINGS(11, "no_report_site_issue", R.string.restriction_disallow_import_settings_title, R.string.restriction_disallow_import_settings_description),
        DISALLOW_TOOLS_MENU(12, "no_tools_menu", R.string.restriction_disallow_tools_menu_title, R.string.restriction_disallow_tools_menu_description),
        DISALLOW_REPORT_SITE_ISSUE(13, "no_report_site_issue", R.string.restriction_disallow_report_site_issue_title, R.string.restriction_disallow_report_site_issue_description);

        public final int id;
        public final String name;
        public final int titleResource;
        public final int descriptionResource;

        Restriction(final int id, final String name, int titleResource, int descriptionResource) {
            this.id = id;
            this.name = name;
            this.titleResource = titleResource;
            this.descriptionResource = descriptionResource;
        }

        public String getTitle(Context context) {
            if (titleResource == 0) {
                return toString();
            }

            return context.getResources().getString(titleResource);
        }

        public String getDescription(Context context) {
            if (descriptionResource == 0) {
                return name;
            }

            return context.getResources().getString(descriptionResource);
        }
    }

    static List<Restriction> GUEST_RESTRICTIONS = Arrays.asList(
        Restriction.DISALLOW_DOWNLOADS,
        Restriction.DISALLOW_INSTALL_EXTENSION,
        Restriction.DISALLOW_INSTALL_APPS,
        Restriction.DISALLOW_BROWSE_FILES,
        Restriction.DISALLOW_SHARE,
        Restriction.DISALLOW_BOOKMARK,
        Restriction.DISALLOW_ADD_CONTACTS,
        Restriction.DISALLOW_SET_IMAGE,
        Restriction.DISALLOW_MODIFY_ACCOUNTS,
        Restriction.DISALLOW_REMOTE_DEBUGGING,
        Restriction.DISALLOW_IMPORT_SETTINGS
    );

    // Restricted profiles will automatically have these restrictions by default
    static List<Restriction> RESTRICTED_PROFILE_RESTRICTIONS = Arrays.asList(
        Restriction.DISALLOW_INSTALL_EXTENSION,
        Restriction.DISALLOW_INSTALL_APPS,
        Restriction.DISALLOW_TOOLS_MENU,
        Restriction.DISALLOW_REPORT_SITE_ISSUE,
        Restriction.DISALLOW_IMPORT_SETTINGS
    );

    private static Restriction geckoActionToRestriction(int action) {
        for (Restriction rest : Restriction.values()) {
            if (rest.id == action) {
                return rest;
            }
        }

        throw new IllegalArgumentException("Unknown action " + action);
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static Bundle getRestrictions(final Context context) {
        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);
        return mgr.getUserRestrictions();
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    private static Bundle getAppRestrictions(final Context context) {
        final UserManager mgr = (UserManager) context.getSystemService(Context.USER_SERVICE);
        return mgr.getApplicationRestrictions(context.getPackageName());
    }

    /**
     * This method does the system version check for you.
     *
     * Returns false if the system doesn't support restrictions,
     * or the provided value is not present in the set of user
     * restrictions.
     *
     * Returns true otherwise.
     */
    private static boolean getRestriction(final Context context, final Restriction restriction) {
        // Early versions don't support restrictions at all,
        // so no action can be restricted.
        if (Versions.preJBMR2) {
            return false;
        }

        if (!isUserRestricted(context)) {
            return false;
        }

        return getAppRestrictions(context).getBoolean(restriction.name, RESTRICTED_PROFILE_RESTRICTIONS.contains(restriction));
    }

    private static boolean canLoadUrl(final Context context, final String url) {
        // Null URLs are always permitted.
        if (url == null) {
            return true;
        }

        try {
            // If we're not in guest mode, and the system restriction isn't in place, everything is allowed.
            if (!getInGuest() &&
                !getRestriction(context, Restriction.DISALLOW_BROWSE_FILES)) {
                return true;
            }
        } catch (IllegalArgumentException ex) {
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

        // TODO: The UserManager should support blacklisting URLs by the device owner.
        return true;
    }

    @WrapElementForJNI
    public static boolean isUserRestricted() {
        return isUserRestricted(GeckoAppShell.getContext());
    }

    public static boolean isUserRestricted(final Context context) {
        // Guest mode is supported in all Android versions.
        if (getInGuest()) {
            return true;
        }

        if (Versions.preJBMR2) {
            return false;
        }

        Bundle restrictions = getRestrictions(context);
        for (String key : restrictions.keySet()) {
            if (restrictions.getBoolean(key)) {
                // At least one restriction is enabled -> We are a restricted profile
                return true;
            }
        }

        return false;
    }

    public static boolean isAllowed(final Context context, final Restriction action) {
        return isAllowed(context, action, null);
    }

    @WrapElementForJNI
    public static boolean isAllowed(int action, String url) {
        return isAllowed(GeckoAppShell.getContext(), action, url);
    }

    private static boolean isAllowed(final Context context, int action, String url) {
        final Restriction restriction;
        try {
            restriction = geckoActionToRestriction(action);
        } catch (IllegalArgumentException ex) {
            // Unknown actions represent a coding error, so we
            // refuse the action and log.
            Log.e(LOGTAG, "Unknown action " + action + "; check calling code.");
            return false;
        }

        return isAllowed(context, restriction, url);
    }

    private static boolean isAllowed(final Context context, final Restriction restriction, String url) {
        if (getInGuest()) {
            if (Restriction.DISALLOW_BROWSE_FILES == restriction) {
                return canLoadUrl(context, url);
            }

            return !GUEST_RESTRICTIONS.contains(restriction);
        }

        // NOTE: Restrictions hold the opposite intention, so we need to flip it.
        return !getRestriction(context, restriction);
    }

    @WrapElementForJNI
    public static String getUserRestrictions() {
        return getUserRestrictions(GeckoAppShell.getContext());
    }

    private static String getUserRestrictions(final Context context) {
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
        final Bundle restrictions = getRestrictions(context);
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
