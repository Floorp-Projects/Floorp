/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.privatebrowsing.controller

import androidx.navigation.NavController
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.privatebrowsing.interactor.PrivateBrowsingInteractor
import org.mozilla.fenix.settings.SupportUtils

/**
 * An interface that handles the view manipulation of the private browsing mode.
 */
interface PrivateBrowsingController {
    /**
     * @see [PrivateBrowsingInteractor.onLearnMoreClicked]
     */
    fun handleLearnMoreClicked()

    /**
     * @see [PrivateBrowsingInteractor.onPrivateModeButtonClicked]
     */
    fun handlePrivateModeButtonClicked(newMode: BrowsingMode)
}

/**
 * The default implementation of [PrivateBrowsingController].
 */
class DefaultPrivateBrowsingController(
    private val activity: HomeActivity,
    private val appStore: AppStore,
    private val navController: NavController,
) : PrivateBrowsingController {

    override fun handleLearnMoreClicked() {
        val learnMoreURL = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS) +
            "?as=u&utm_source=inproduct"

        activity.openToBrowserAndLoad(
            searchTermOrURL = learnMoreURL,
            newTab = true,
            from = BrowserDirection.FromHome,
        )
    }

    override fun handlePrivateModeButtonClicked(newMode: BrowsingMode) {
        if (newMode == BrowsingMode.Private) {
            activity.settings().incrementNumTimesPrivateModeOpened()
        }

        appStore.dispatch(
            AppAction.ModeChange(newMode),
        )

        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalSearchDialog(
                    sessionId = null,
                ),
            )
        }
    }
}
