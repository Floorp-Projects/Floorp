/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

/**
 * Contract declaring all possible user interactions with [CookieBannerHandlingDetailsView].
 */
interface CookieBannerDetailsInteractor {
    /**
     * Called whenever back is pressed.
     */
    fun onBackPressed() = Unit

    /**
     * Called whenever the user press the toggle widget.
     */
    fun onTogglePressed(vale: Boolean) = Unit

    /**
     * Called whenever the user press request support site domain.
     */
    fun handleRequestSiteSupportPressed()
}

/**
 * [CookieBannerPanelDialogFragment] interactor.
 *
 * Implements callbacks for each of [CookieBannerPanelDialogFragment]'s Views declared possible user interactions,
 * delegates all such user events to the [CookieBannerDetailsController].
 *
 * @param controller [CookieBannerDetailsController] which will be delegated for all users interactions,
 * it expected to contain all business logic for how to act in response.
 */
class DefaultCookieBannerDetailsInteractor(
    private val controller: CookieBannerDetailsController,
) : CookieBannerDetailsInteractor {

    override fun onBackPressed() {
        controller.handleBackPressed()
    }

    override fun onTogglePressed(vale: Boolean) {
        controller.handleTogglePressed(vale)
    }

    override fun handleRequestSiteSupportPressed() {
        controller.handleRequestSiteSupportPressed()
    }
}
