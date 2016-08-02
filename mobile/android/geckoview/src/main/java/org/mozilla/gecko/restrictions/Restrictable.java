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
public enum Restrictable {
    DOWNLOAD(1, "downloads", 0, 0),

    INSTALL_EXTENSION(
            2, "no_install_extensions",
            R.string.restrictable_feature_addons_installation,
            R.string.restrictable_feature_addons_installation_description),

    // UserManager.DISALLOW_INSTALL_APPS
    INSTALL_APPS(3, "no_install_apps", 0 , 0),

    BROWSE(4, "browse", 0, 0),

    SHARE(5, "share", 0, 0),

    BOOKMARK(6, "bookmark", 0, 0),

    ADD_CONTACT(7, "add_contact", 0, 0),

    SET_IMAGE(8, "set_image", 0, 0),

    // UserManager.DISALLOW_MODIFY_ACCOUNTS
    MODIFY_ACCOUNTS(9, "no_modify_accounts", 0, 0),

    REMOTE_DEBUGGING(10, "remote_debugging", 0, 0),

    IMPORT_SETTINGS(11, "import_settings", 0, 0),

    PRIVATE_BROWSING(
            12, "private_browsing",
            R.string.restrictable_feature_private_browsing,
            R.string.restrictable_feature_private_browsing_description),

    DATA_CHOICES(13, "data_coices", 0, 0),

    CLEAR_HISTORY(14, "clear_history",
            R.string.restrictable_feature_clear_history,
            R.string.restrictable_feature_clear_history_description),

    MASTER_PASSWORD(15, "master_password", 0, 0),

    GUEST_BROWSING(16, "guest_browsing",  0, 0),

    ADVANCED_SETTINGS(17, "advanced_settings",
            R.string.restrictable_feature_advanced_settings,
            R.string.restrictable_feature_advanced_settings_description),

    CAMERA_MICROPHONE(18, "camera_microphone",
            R.string.restrictable_feature_camera_microphone,
            R.string.restrictable_feature_camera_microphone_description),

    BLOCK_LIST(19, "block_list",
            R.string.restrictable_feature_block_list,
            R.string.restrictable_feature_block_list_description),

    TELEMETRY(20, "telemetry",
            R.string.datareporting_telemetry_title,
            R.string.datareporting_telemetry_summary),

    HEALTH_REPORT(21, "health_report",
            R.string.datareporting_fhr_title,
            R.string.datareporting_fhr_summary2),

    DEFAULT_THEME(22, "default_theme", 0, 0);

    public final int id;
    public final String name;

    @StringRes
    public final int title;

    @StringRes
    public final int description;

    Restrictable(final int id, final String name, @StringRes int title, @StringRes int description) {
        this.id = id;
        this.name = name;
        this.title = title;
        this.description = description;
    }

    public String getTitle(Context context) {
        if (title == 0) {
            return toString();
        }
        return context.getResources().getString(title);
    }

    public String getDescription(Context context) {
        if (description == 0) {
            return null;
        }
        return context.getResources().getString(description);
    }
}
