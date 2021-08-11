/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.SessionUseCases
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito
import org.mockito.Mockito.times
import org.mockito.MockitoAnnotations
import org.mockito.Spy
import org.mozilla.focus.browser.integration.BrowserMenuController
import org.mozilla.focus.state.AppStore
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserMenuControllerTest {
    private lateinit var browserMenuController: BrowserMenuController

    @Spy
    private lateinit var sessionUseCases: SessionUseCases

    @Mock
    private lateinit var appStore: AppStore

    private val currentTabId: String = "1"
    private val selectedTab = createTab("https://www.mozilla.org", id = "1")
    private val shareCallback: () -> Unit = {}

    @Mock
    private lateinit var requestDesktopCallback: (isChecked: Boolean) -> Unit

    @Mock
    private lateinit var addToHomeScreenCallback: () -> Unit

    @Mock
    private lateinit var showFindInPageCallback: () -> Unit

    @Mock
    private lateinit var openInCallback: () -> Unit

    // NB: we should avoid mocking lambdas..
    @Mock
    private lateinit var openInBrowser: () -> Unit

    @Before
    fun setup() {
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(selectedTab),
                selectedTabId = selectedTab.id
            )
        )
        sessionUseCases = SessionUseCases(store)
        MockitoAnnotations.openMocks(this)

        browserMenuController = BrowserMenuController(
            sessionUseCases,
            appStore,
            currentTabId,
            shareCallback,
            requestDesktopCallback,
            addToHomeScreenCallback,
            showFindInPageCallback,
            openInCallback,
            openInBrowser
        )
    }

    @Test
    fun `GIVEN Back menu item WHEN the item is pressed THEN goBack use case is called`() {
        val menuItem = ToolbarMenu.Item.Back
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(sessionUseCases, times(1)).goBack
    }

    @Test
    fun `GIVEN Forward menu item WHEN the item is pressed THEN goForward use case is called`() {
        val menuItem = ToolbarMenu.Item.Forward
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(sessionUseCases, times(1)).goForward
    }

    @Test
    fun `GIVEN Reload menu item WHEN the item is pressed THEN reload use case is called`() {
        val menuItem = ToolbarMenu.Item.Reload
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(sessionUseCases, times(1)).reload
    }

    @Test
    fun `GIVEN Stop menu item WHEN the item is pressed THEN stopLoading use case is called`() {
        val menuItem = ToolbarMenu.Item.Stop
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(sessionUseCases, times(1)).stopLoading
    }

    @Test
    @Suppress("MaxLineLength")
    fun `GIVEN RequestDesktop menu item WHEN the item is switched to false THEN requestDesktopCallback with false is called`() {
        val menuItem = ToolbarMenu.Item.RequestDesktop(isChecked = false)
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(requestDesktopCallback, times(1)).invoke(false)
    }

    @Test
    @Suppress("MaxLineLength")
    fun `GIVEN RequestDesktop menu item WHEN the item is switched to true THEN requestDesktopCallback with true is called`() {
        val menuItem = ToolbarMenu.Item.RequestDesktop(isChecked = true)
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(requestDesktopCallback, times(1)).invoke(true)
    }

    @Test
    fun `GIVEN OpenInBrowser menu item WHEN the item is pressed THEN openInBrowser is called`() {
        val menuItem = ToolbarMenu.Item.OpenInBrowser
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(openInBrowser, times(1)).invoke()
    }

    @Test
    fun `GIVEN OpenIn menu item WHEN the item is pressed THEN openInCallback is called`() {
        val menuItem = ToolbarMenu.Item.OpenInApp
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(openInCallback, times(1)).invoke()
    }

    @Test
    fun `GIVEN FindInPage menu item WHEN the item is pressed THEN findInPageMenuEvent method is called`() {
        val menuItem = ToolbarMenu.Item.FindInPage
        browserMenuController.handleMenuInteraction(menuItem)
        Mockito.verify(showFindInPageCallback, times(1)).invoke()
    }
}
