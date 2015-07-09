/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.home.HomeConfig.PanelType;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.util.StringUtils;

public class AboutPages {
    // All of our special pages.
    public static final String ADDONS          = "about:addons";
    public static final String CONFIG          = "about:config";
    public static final String DOWNLOADS       = "about:downloads";
    public static final String FIREFOX         = "about:firefox";
    public static final String HEALTHREPORT    = "about:healthreport";
    public static final String HOME            = "about:home";
    public static final String LOGINS          = "about:logins";
    public static final String PRIVATEBROWSING = "about:privatebrowsing";
    public static final String READER          = "about:reader";
    public static final String UPDATER         = "about:";

    public static final String URL_FILTER = "about:%";

    public static final String PANEL_PARAM = "panel";

    public static final boolean isAboutPage(final String url) {
        return url != null && url.startsWith("about:");
    }

    public static final boolean isTitlelessAboutPage(final String url) {
        return isAboutHome(url) ||
               PRIVATEBROWSING.equals(url);
    }

    public static final boolean isAboutHome(final String url) {
        if (url == null || !url.startsWith(HOME)) {
            return false;
        }
        // We sometimes append a parameter to "about:home" to specify which page to
        // show when we open the home pager. Discard this parameter when checking
        // whether or not this URL is "about:home".
        return HOME.equals(url.split("\\?")[0]);
    }

    public static final String getPanelIdFromAboutHomeUrl(String aboutHomeUrl) {
        return StringUtils.getQueryParameter(aboutHomeUrl, PANEL_PARAM);
    }

    public static final boolean isAboutReader(final String url) {
        if (url == null) {
            return false;
        }
        return url.startsWith(READER);
    }

    private static final String[] DEFAULT_ICON_PAGES = new String[] {
        ADDONS,
        CONFIG,
        DOWNLOADS,
        FIREFOX,
        HEALTHREPORT,
        UPDATER
    };

    /**
     * Callers must not modify the returned array.
     */
    public static String[] getDefaultIconPages() {
        return DEFAULT_ICON_PAGES;
    }

    public static boolean isBuiltinIconPage(final String url) {
        if (url == null ||
            !url.startsWith("about:")) {
            return false;
        }

        // about:home uses a separate search built-in icon.
        if (isAboutHome(url)) {
            return true;
        }

        // TODO: it'd be quicker to not compare the "about:" part every time.
        for (int i = 0; i < DEFAULT_ICON_PAGES.length; ++i) {
            if (DEFAULT_ICON_PAGES[i].equals(url)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Get a URL that navigates to the specified built-in Home Panel.
     *
     * @param panelType to navigate to.
     * @return URL.
     * @throws IllegalArgumentException if the built-in panel type is not a built-in panel.
     */
    @RobocopTarget
    public static String getURLForBuiltinPanelType(PanelType panelType) throws IllegalArgumentException {
        return HOME + "?panel=" + HomeConfig.getIdForBuiltinPanelType(panelType);
    }
}
