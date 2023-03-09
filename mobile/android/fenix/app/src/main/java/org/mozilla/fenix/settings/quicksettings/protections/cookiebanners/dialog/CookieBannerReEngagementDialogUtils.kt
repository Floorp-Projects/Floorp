/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.dialog

import androidx.navigation.NavController
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingMode.REJECT_ALL
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingStatus
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.Settings
import mozilla.components.concept.engine.Settings as EngineSettings

/**
 *   An utility object for interacting with the re-engagement cookie banner dialog.
 */
object CookieBannerReEngagementDialogUtils {
    /**
     *  Tries to show the re-engagement cookie banner dialog, when the right conditions are met, o
     *  otherwise the dialog won't show.
     */
    fun tryToShowReEngagementDialog(
        settings: Settings,
        status: CookieBannerHandlingStatus,
        navController: NavController,
    ) {
        if (status == CookieBannerHandlingStatus.DETECTED &&
            settings.shouldShowCookieBannerReEngagementDialog()
        ) {
            settings.cookieBannerReEngagementDialogShowsCount.increment()
            settings.lastInteractionWithReEngageCookieBannerDialogInMs = System.currentTimeMillis()
            settings.cookieBannerDetectedPreviously = true
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToCookieBannerDialogFragment()
            navController.nav(R.id.browserFragment, directions)
        }
    }

    /**
     *  Tries to enable the detect only mode after the time limit for the cookie banner has been
     *  expired.
     */
    fun tryToEnableDetectOnlyModeIfNeeded(
        settings: Settings,
        engineSettings: EngineSettings,
    ) {
        if (settings.shouldShowCookieBannerReEngagementDialog()) {
            engineSettings.cookieBannerHandlingDetectOnlyMode = true
            engineSettings.cookieBannerHandlingModePrivateBrowsing = REJECT_ALL
            engineSettings.cookieBannerHandlingMode = REJECT_ALL
        }
    }
}
