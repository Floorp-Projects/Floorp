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
    DOWNLOAD(1, "downloads", 0),

    INSTALL_EXTENSION(2, "no_install_extensions", R.string.restrictable_feature_addons_installation),

    // UserManager.DISALLOW_INSTALL_APPS
    INSTALL_APPS(3, "no_install_apps", 0),

    BROWSE(4, "browse", 0),

    SHARE(5, "share", 0),

    BOOKMARK(6, "bookmark", 0),

    ADD_CONTACT(7, "add_contact", 0),

    SET_IMAGE(8, "set_image", 0),

    // UserManager.DISALLOW_MODIFY_ACCOUNTS
    MODIFY_ACCOUNTS(9, "no_modify_accounts", 0),

    REMOTE_DEBUGGING(10, "remote_debugging", 0),

    IMPORT_SETTINGS(11, "import_settings", 0),

    PRIVATE_BROWSING(12, "private_browsing", R.string.restrictable_feature_private_browsing),

    LOCATION_SERVICE(13, "location_service", R.string.restrictable_feature_location_services),

    CLEAR_HISTORY(14, "clear_history", R.string.restrictable_feature_clear_history),

    MASTER_PASSWORD(15, "master_password", R.string.restrictable_feature_master_password),

    GUEST_BROWSING(16, "guest_browsing",  R.string.restrictable_feature_guest_browsing),

    ADVANCED_SETTINGS(17, "advanced_settings", R.string.restrictable_feature_advanced_settings),

    CAMERA_MICROPHONE(18, "camera_microphone", R.string.restrictable_feature_camera_microphone);

    public final int id;
    public final String name;

    @StringRes
    public final int title;

    Restrictable(final int id, final String name, @StringRes int title) {
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
