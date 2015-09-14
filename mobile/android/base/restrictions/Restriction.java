/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import org.mozilla.gecko.R;

import android.content.Context;
import android.support.annotation.StringRes;

/**
 * This is a list of things we can restrict you from doing. Some of these are reflected in Android UserManager constants.
 * Others are specific to us.
 * These constants should be in sync with the ones from toolkit/components/parentalcontrols/nsIParentalControlsService.idl
 */
public enum Restriction {
    DISALLOW_DOWNLOADS(1, "no_download_files", 0),

    DISALLOW_INSTALL_EXTENSION(2, "no_install_extensions", R.string.restriction_disallow_addons_title),

    // UserManager.DISALLOW_INSTALL_APPS
    DISALLOW_INSTALL_APPS(3, "no_install_apps", 0),

    DISALLOW_BROWSE_FILES(4, "no_browse_files", 0),

    DISALLOW_SHARE(5, "no_share", 0),

    DISALLOW_BOOKMARK(6, "no_bookmark", 0),

    DISALLOW_ADD_CONTACTS(7, "no_add_contacts", 0),

    DISALLOW_SET_IMAGE(8, "no_set_image", 0),

    // UserManager.DISALLOW_MODIFY_ACCOUNTS
    DISALLOW_MODIFY_ACCOUNTS(9, "no_modify_accounts", 0),

    DISALLOW_REMOTE_DEBUGGING(10, "no_remote_debugging", 0),

    DISALLOW_IMPORT_SETTINGS(11, "no_import_settings", R.string.restriction_disallow_import_settings_title),

    DISALLOW_DEVELOPER_TOOLS(12, "no_developer_tools", R.string.restriction_disallow_devtools_title),

    DISALLOW_CUSTOMIZE_HOME(13, "no_customize_home", R.string.restriction_disallow_customize_home_title),

    DISALLOW_PRIVATE_BROWSING(14, "no_private_browsing", R.string.restriction_disallow_private_browsing_title),

    DISALLOW_LOCATION_SERVICE(15, "no_location_service", R.string.restriction_disallow_location_services_title),

    DISALLOW_DISPLAY_SETTINGS(16, "no_display_settings", R.string.restriction_disallow_display_settings_title),

    DISALLOW_CLEAR_HISTORY(17, "no_clear_history", R.string.restriction_disallow_clear_history_title),

    DISALLOW_MASTER_PASSWORD(18, "no_master_password", R.string.restriction_disallow_master_password_title),

    DISALLOW_GUEST_BROWSING(19, "no_guest_browsing",  R.string.restriction_disallow_guest_browsing_title),

    DISALLOW_DEFAULT_THEME(20, "no_default_theme", 0);

    public final int id;
    public final String name;

    @StringRes
    public final int title;

    Restriction(final int id, final String name, @StringRes int title) {
        this.id = id;
        this.name = name;
        this.title = title;
    }

    public String getTitle(Context context) {
        if (title == 0) {
            return toString();
        }
        return context.getResources().getString(title);
    }
}
