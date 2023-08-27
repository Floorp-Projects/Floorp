/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.toolbar.interactor.DefaultBrowserToolbarInteractor

class DefaultBrowserToolbarInteractorTest {

    @RelaxedMockK lateinit var browserToolbarController: BrowserToolbarController

    @RelaxedMockK lateinit var browserToolbarMenuController: BrowserToolbarMenuController
    lateinit var interactor: DefaultBrowserToolbarInteractor

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = DefaultBrowserToolbarInteractor(
            browserToolbarController,
            browserToolbarMenuController,
        )
    }

    @Test
    fun onTabCounterClicked() {
        interactor.onTabCounterClicked()
        verify { browserToolbarController.handleTabCounterClick() }
    }

    @Test
    fun onTabCounterMenuItemTapped() {
        val item: TabCounterMenu.Item = mockk()

        interactor.onTabCounterMenuItemTapped(item)
        verify { browserToolbarController.handleTabCounterItemInteraction(item) }
    }

    @Test
    fun onBrowserToolbarPaste() {
        val pastedText = "Mozilla"
        interactor.onBrowserToolbarPaste(pastedText)
        verify { browserToolbarController.handleToolbarPaste(pastedText) }
    }

    @Test
    fun onBrowserToolbarPasteAndGo() {
        val pastedText = "Mozilla"

        interactor.onBrowserToolbarPasteAndGo(pastedText)
        verify { browserToolbarController.handleToolbarPasteAndGo(pastedText) }
    }

    @Test
    fun onBrowserToolbarClicked() {
        interactor.onBrowserToolbarClicked()

        verify { browserToolbarController.handleToolbarClick() }
    }

    @Test
    fun onBrowserToolbarMenuItemTapped() {
        val item: ToolbarMenu.Item = mockk()

        interactor.onBrowserToolbarMenuItemTapped(item)

        verify { browserToolbarMenuController.handleToolbarItemInteraction(item) }
    }

    @Test
    fun onHomeButtonClicked() {
        interactor.onHomeButtonClicked()

        verify { browserToolbarController.handleHomeButtonClick() }
    }

    @Test
    fun onEraseButtonClicked() {
        interactor.onEraseButtonClicked()

        verify { browserToolbarController.handleEraseButtonClick() }
    }

    @Test
    fun onShoppingCfrActionClicked() {
        interactor.onShoppingCfrActionClicked()

        verify { browserToolbarController.handleShoppingCfrActionClick() }
    }

    @Test
    fun onTranslationsButtonClicked() {
        interactor.onTranslationsButtonClicked()

        verify { browserToolbarController.handleTranslationsButtonClick() }
    }
}
