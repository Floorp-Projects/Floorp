package org.mozilla.fenix.components.toolbar

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class BrowserInteractorTest {

    lateinit var browserToolbarController: BrowserToolbarController
    lateinit var interactor: BrowserInteractor

    @Before
    fun setup() {
        browserToolbarController = mockk(relaxed = true)
        interactor = BrowserInteractor(
            browserToolbarController
        )
    }

    @Test
    fun onTabCounterClicked() {
        interactor.onTabCounterClicked()
        verify { browserToolbarController.handleTabCounterClick() }
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

        verify { browserToolbarController.handleToolbarItemInteraction(item) }
    }
}
