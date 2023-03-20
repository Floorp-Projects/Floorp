/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.home.privatebrowsing.controller.PrivateBrowsingController
import org.mozilla.fenix.home.toolbar.ToolbarController
import org.mozilla.fenix.onboarding.controller.OnboardingController
import org.mozilla.fenix.onboarding.interactor.DefaultOnboardingInteractor
import org.mozilla.fenix.search.toolbar.SearchSelectorController
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu

class DefaultOnboardingInteractorTest {

    private val controller: OnboardingController = mockk(relaxed = true)
    private val privateBrowsingController: PrivateBrowsingController = mockk(relaxed = true)
    private val searchSelectorController: SearchSelectorController = mockk(relaxed = true)
    private val toolbarController: ToolbarController = mockk(relaxed = true)

    private lateinit var interactor: DefaultOnboardingInteractor

    @Before
    fun setup() {
        interactor = DefaultOnboardingInteractor(
            controller = controller,
            privateBrowsingController = privateBrowsingController,
            searchSelectorController = searchSelectorController,
            toolbarController = toolbarController,
        )
    }

    @Test
    fun `WHEN the onboarding is finished THEN forward to controller handler`() {
        val focusOnAddressBar = true
        interactor.onFinishOnboarding(focusOnAddressBar)
        verify { controller.handleFinishOnboarding(focusOnAddressBar) }
    }

    @Test
    fun `WHEN the privacy notice clicked THEN forward to controller handler`() {
        interactor.onReadPrivacyNoticeClicked()
        verify { controller.handleReadPrivacyNoticeClicked() }
    }

    @Test
    fun onMenuItemTapped() {
        val item = SearchSelectorMenu.Item.SearchSettings
        interactor.onMenuItemTapped(item)
        verify { searchSelectorController.handleMenuItemTapped(item) }
    }

    @Test
    fun onLearnMoreClicked() {
        interactor.onLearnMoreClicked()
        verify { privateBrowsingController.handleLearnMoreClicked() }
    }

    @Test
    fun onPrivateModeButtonClicked() {
        val newMode = BrowsingMode.Private
        val hasBeenOnboarded = true

        interactor.onPrivateModeButtonClicked(newMode, hasBeenOnboarded)
        verify { privateBrowsingController.handlePrivateModeButtonClicked(newMode, hasBeenOnboarded) }
    }

    @Test
    fun onNavigateSearch() {
        interactor.onNavigateSearch()
        verify { toolbarController.handleNavigateSearch() }
    }

    @Test
    fun onPaste() {
        val clipboardText = "text"
        interactor.onPaste(clipboardText)
        verify { toolbarController.handlePaste(clipboardText) }
    }

    @Test
    fun onPasteAndGo() {
        val clipboardText = "text"
        interactor.onPasteAndGo(clipboardText)
        verify { toolbarController.handlePasteAndGo(clipboardText) }
    }
}
