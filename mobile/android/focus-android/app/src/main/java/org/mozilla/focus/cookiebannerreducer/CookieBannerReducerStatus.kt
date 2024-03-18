/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

/**
 * Sealed class for the cookie banner exception Gui item
 * from Tracking Protection panel.
 */
sealed class CookieBannerReducerStatus {

    /**
     * If the site is excepted from cookie banner reduction.
     */
    object HasException : CookieBannerReducerStatus()

    /**
     * If the site is not excepted from cookie banner reduction.
     */
    object NoException : CookieBannerReducerStatus()

    /**
     * If the cookie banner reducer is not supported on the site.
     */
    object CookieBannerSiteNotSupported : CookieBannerReducerStatus()

    /**
     * If the user reports with success a site that wasn't supported by cookie banner reducer.
     */
    object CookieBannerUnsupportedSiteRequestWasSubmitted : CookieBannerReducerStatus()
}
