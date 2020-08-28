package org.mozilla.fenix.components.toolbar

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class BrowserInteractorTest {

    @RelaxedMockK lateinit var browserToolbarController: BrowserToolbarController
    @RelaxedMockK lateinit var browserToolbarMenuController: BrowserToolbarMenuController
    lateinit var interactor: BrowserInteractor

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = BrowserInteractor(
            browserToolbarController,
            browserToolbarMenuController
        )
    }

    @Test
    fun onTabCounterClicked() {
        interactor.onTabCounterClicked()
        verify { browserToolbarController.handleTabCounterClick() }
    }

    @Test
    fun onTabCounterMenuItemTapped() {
        val item: TabCounterMenuItem = mockk()

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
}
