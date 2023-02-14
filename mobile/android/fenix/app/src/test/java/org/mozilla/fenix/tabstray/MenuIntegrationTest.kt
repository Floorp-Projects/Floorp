/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test

class MenuIntegrationTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val captureMiddleware = CaptureActionsMiddleware<TabsTrayState, TabsTrayAction>()
    private val tabsTrayStore = TabsTrayStore(middlewares = listOf(captureMiddleware))
    private val interactor = mockk<NavigationInteractor>(relaxed = true)

    @Test
    fun `WHEN the share all menu item is clicked THEN invoke the action`() {
        val menu = MenuIntegration(mockk(), mockk(), tabsTrayStore, mockk(), interactor)

        menu.handleMenuClicked(TabsTrayMenu.Item.ShareAllTabs)

        verify { interactor.onShareTabsOfTypeClicked(false) }
    }

    @Test
    fun `WHEN the open account settings menu item is clicked THEN invoke the action`() {
        val menu = MenuIntegration(mockk(), mockk(), tabsTrayStore, mockk(), interactor)

        menu.handleMenuClicked(TabsTrayMenu.Item.OpenAccountSettings)

        verify { interactor.onAccountSettingsClicked() }
    }

    @Test
    fun `WHEN the open settings menu item is clicked THEN invoke the action`() {
        val menu = MenuIntegration(mockk(), mockk(), tabsTrayStore, mockk(), interactor)

        menu.handleMenuClicked(TabsTrayMenu.Item.OpenTabSettings)

        verify { interactor.onTabSettingsClicked() }
    }

    @Test
    fun `WHEN the close all menu item is clicked THEN invoke the action`() {
        val menu = MenuIntegration(mockk(), mockk(), tabsTrayStore, mockk(), interactor)

        menu.handleMenuClicked(TabsTrayMenu.Item.CloseAllTabs)

        verify { interactor.onCloseAllTabsClicked(false) }
    }

    @Test
    fun `WHEN the recently menu item is clicked THEN invoke the action`() {
        val menu = MenuIntegration(mockk(), mockk(), tabsTrayStore, mockk(), interactor)

        menu.handleMenuClicked(TabsTrayMenu.Item.OpenRecentlyClosed)

        verify { interactor.onOpenRecentlyClosedClicked() }
    }

    @Test
    fun `WHEN the select menu item is clicked THEN invoke the action`() {
        val menu = MenuIntegration(mockk(), mockk(), tabsTrayStore, mockk(), interactor)

        menu.handleMenuClicked(TabsTrayMenu.Item.SelectTabs)

        tabsTrayStore.waitUntilIdle()

        assertNotNull(captureMiddleware.findLastAction(TabsTrayAction.EnterSelectMode::class))
    }
}
