/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.cookiebannerreducer

import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.CookieBanner

/**
 * Interactor class for cookie banner reducer feature .
 */
class DefaultCookieBannerReducerInteractor(val store: CookieBannerReducerStore) {

    /**
     * Method that gets called when the user changes the cookie banner exception toggle state.
     * @param isCookieBannerHandlingExceptionEnabled - the state of the toggle
     */
    fun handleToggleCookieBannerException(isCookieBannerHandlingExceptionEnabled: Boolean) {
        store.dispatch(
            CookieBannerReducerAction.ToggleCookieBannerException(
                isCookieBannerHandlingExceptionEnabled,
            ),
        )
    }

    /**
     * Method that gets called when the user sends the url of the site that he wants to report.
     * @param siteDomain - the site domain that will be sent to nimbus.
     */
    fun handleRequestReportSiteDomain(siteDomain: String) {
        CookieBanner.reportDomainSiteButton.record(NoExtras())
        store.dispatch(CookieBannerReducerAction.RequestReportSite(siteToReport = siteDomain))
    }
}
