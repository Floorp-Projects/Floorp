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
    static List<Restrictable> DEFAULT_RESTRICTIONS = Arrays.asList(
            Restrictable.DISALLOW_DOWNLOADS,
            Restrictable.DISALLOW_INSTALL_EXTENSION,
            Restrictable.DISALLOW_INSTALL_APPS,
            Restrictable.DISALLOW_BROWSE_FILES,
            Restrictable.DISALLOW_SHARE,
            Restrictable.DISALLOW_BOOKMARK,
            Restrictable.DISALLOW_ADD_CONTACTS,
            Restrictable.DISALLOW_SET_IMAGE,
            Restrictable.DISALLOW_MODIFY_ACCOUNTS,
            Restrictable.DISALLOW_REMOTE_DEBUGGING,
            Restrictable.DISALLOW_IMPORT_SETTINGS
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
        return !DEFAULT_RESTRICTIONS.contains(restrictable);
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
