/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.pwa;

import android.support.annotation.CheckResult;

import org.mozilla.gecko.Tab;


public class PwaUtils {
    /**
     * Check if this tab should add as PWA shortcut
     * From the document :https://developers.google.com/web/fundamentals/security/encrypt-in-transit/why-https
     * PWA must be served from a secure origin. We also restrict it to
     * 1. Not allow user accept exception
     * 2. The tab can't be a private tab.
     *
     * @param tab Since website info is in the tab, we pass the tab here.
     * @return true if the tab shouldAddPwaShortcut
     */
    @CheckResult
    public static boolean shouldAddPwaShortcut(Tab tab) {
        final boolean secure = tab.getSiteIdentity().isSecure();
        // This tab is safe for pwa only when the site is absolutely secure.
        // so no exception is allowed
        final boolean exception = tab.getSiteIdentity().isSecurityException();
        return !tab.isPrivate() && secure && !exception;
    }
}
