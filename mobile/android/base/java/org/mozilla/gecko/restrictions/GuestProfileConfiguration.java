/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.restrictions;

import android.net.Uri;

import java.util.Arrays;
import java.util.List;

/**
 * RestrictionConfiguration implementation for guest profiles.
 */
public class GuestProfileConfiguration implements RestrictionConfiguration {
    static List<Restrictable> DISABLED_FEATURES = Arrays.asList(
            Restrictable.DOWNLOAD,
            Restrictable.INSTALL_EXTENSION,
            Restrictable.INSTALL_APPS,
            Restrictable.BROWSE,
            Restrictable.SHARE,
            Restrictable.BOOKMARK,
            Restrictable.ADD_CONTACT,
            Restrictable.SET_IMAGE,
            Restrictable.MODIFY_ACCOUNTS,
            Restrictable.REMOTE_DEBUGGING,
            Restrictable.IMPORT_SETTINGS,
            Restrictable.BLOCK_LIST,
            Restrictable.DATA_CHOICES,
            Restrictable.DEFAULT_THEME
    );

    @SuppressWarnings("serial")
    private static final List<String> BANNED_SCHEMES = Arrays.asList(
            "file",
            "chrome",
            "resource",
            "jar",
            "wyciwyg"
    );

    private static final List<String> BANNED_URLS = Arrays.asList(
            "about:config",
            "about:addons"
    );

    @Override
    public boolean isAllowed(Restrictable restrictable) {
        return !DISABLED_FEATURES.contains(restrictable);
    }

    @Override
    public boolean canLoadUrl(String url) {
        // Null URLs are always permitted.
        if (url == null) {
            return true;
        }

        final Uri u = Uri.parse(url);
        final String scheme = u.getScheme();
        if (BANNED_SCHEMES.contains(scheme)) {
            return false;
        }

        url = url.toLowerCase();
        for (String banned : BANNED_URLS) {
            if (url.startsWith(banned)) {
                return false;
            }
        }

        return true;
    }

    @Override
    public boolean isRestricted() {
        return true;
    }

    @Override
    public void update() {}
}
