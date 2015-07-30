/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import org.mozilla.gecko.R;

import android.content.Context;

/**
 * This is a list of things we can restrict you from doing. Some of these are reflected in Android UserManager constants.
 * Others are specific to us.
 * These constants should be in sync with the ones from toolkit/components/parentalcontrols/nsIParentalControlsService.idl
 */
public enum Restriction {
    DISALLOW_DOWNLOADS(
            1, "no_download_files", 0, 0),

    DISALLOW_INSTALL_EXTENSION(
            2, "no_install_extensions",
            R.string.restriction_disallow_addons_title,
            R.string.restriction_disallow_addons_description),

    // UserManager.DISALLOW_INSTALL_APPS
    DISALLOW_INSTALL_APPS(
            3, "no_install_apps",
            R.string.restriction_disallow_apps_title,
            R.string.restriction_disallow_apps_description),

    DISALLOW_BROWSE_FILES(
            4, "no_browse_files", 0, 0),

    DISALLOW_SHARE(
            5, "no_share", 0, 0),

    DISALLOW_BOOKMARK(
            6, "no_bookmark", 0, 0),

    DISALLOW_ADD_CONTACTS(
            7, "no_add_contacts", 0, 0),

    DISALLOW_SET_IMAGE(
            8, "no_set_image", 0, 0),

    // UserManager.DISALLOW_MODIFY_ACCOUNTS
    DISALLOW_MODIFY_ACCOUNTS(
            9, "no_modify_accounts", 0, 0),

    DISALLOW_REMOTE_DEBUGGING(
            10, "no_remote_debugging", 0, 0),

    DISALLOW_IMPORT_SETTINGS(
            11, "no_report_site_issue",
            R.string.restriction_disallow_import_settings_title,
            R.string.restriction_disallow_import_settings_description),

    DISALLOW_TOOLS_MENU(
            12, "no_tools_menu",
            R.string.restriction_disallow_tools_menu_title,
            R.string.restriction_disallow_tools_menu_description),

    DISALLOW_REPORT_SITE_ISSUE(
            13, "no_report_site_issue",
            R.string.restriction_disallow_report_site_issue_title,
            R.string.restriction_disallow_report_site_issue_description),

    DISALLOW_DEVELOPER_TOOLS(
            14, "no_developer_tools",
            R.string.restriction_disallow_devtools_title,
            R.string.restriction_disallow_devtools_description
    ),

    DISALLOW_CUSTOMIZE_HOME(
            15, "no_customize_home",
            R.string.restriction_disallow_customize_home_title,
            R.string.restriction_disallow_customize_home_description
    ),

    DISALLOW_PRIVATE_BROWSING(
            16, "no_private_browsing",
            R.string.restriction_disallow_private_browsing_title,
            R.string.restriction_disallow_private_browsing_description
    );

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
