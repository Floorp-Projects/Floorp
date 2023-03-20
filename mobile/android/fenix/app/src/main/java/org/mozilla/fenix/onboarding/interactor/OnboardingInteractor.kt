/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.interactor

import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.HomeMenu
import org.mozilla.fenix.home.privatebrowsing.controller.PrivateBrowsingController
import org.mozilla.fenix.home.privatebrowsing.interactor.PrivateBrowsingInteractor
import org.mozilla.fenix.home.toolbar.ToolbarController
import org.mozilla.fenix.home.toolbar.ToolbarInteractor
import org.mozilla.fenix.onboarding.controller.OnboardingController
import org.mozilla.fenix.search.toolbar.SearchSelectorController
import org.mozilla.fenix.search.toolbar.SearchSelectorInteractor
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu

/**
 * Interface for onboarding related actions.
 */
interface OnboardingInteractor {
    /**
     * Finishes the onboarding and navigates to the [HomeFragment]. Called when a user clicks on the
     * "Start Browsing" button or a menu item in the [HomeMenu].
     *
     * @param focusOnAddressBar Whether or not to focus the address bar when navigating to the
     * [HomeFragment].
     */
    fun onFinishOnboarding(focusOnAddressBar: Boolean)

    /**
     * Opens a custom tab to privacy notice url. Called when a user clicks on the "read our privacy notice" button.
     */
    fun onReadPrivacyNoticeClicked()
}

/**
 * The default implementation of [OnboardingInteractor].
 *
 * @param controller An instance of [OnboardingController] which will be delegated for all user
 * interactions.
 */
class DefaultOnboardingInteractor(
    private val controller: OnboardingController,
    private val privateBrowsingController: PrivateBrowsingController,
    private val searchSelectorController: SearchSelectorController,
    private val toolbarController: ToolbarController,
) : OnboardingInteractor,
    PrivateBrowsingInteractor,
    SearchSelectorInteractor,
    ToolbarInteractor {

    override fun onFinishOnboarding(focusOnAddressBar: Boolean) {
        controller.handleFinishOnboarding(focusOnAddressBar)
    }

    override fun onReadPrivacyNoticeClicked() {
        controller.handleReadPrivacyNoticeClicked()
    }

    override fun onMenuItemTapped(item: SearchSelectorMenu.Item) {
        searchSelectorController.handleMenuItemTapped(item)
    }

    override fun onLearnMoreClicked() {
        privateBrowsingController.handleLearnMoreClicked()
    }

    override fun onPrivateModeButtonClicked(newMode: BrowsingMode, userHasBeenOnboarded: Boolean) {
        privateBrowsingController.handlePrivateModeButtonClicked(newMode, userHasBeenOnboarded)
    }

    override fun onNavigateSearch() {
        toolbarController.handleNavigateSearch()
    }

    override fun onPaste(clipboardText: String) {
        toolbarController.handlePaste(clipboardText)
    }

    override fun onPasteAndGo(clipboardText: String) {
        toolbarController.handlePasteAndGo(clipboardText)
    }
}
