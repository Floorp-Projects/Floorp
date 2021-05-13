/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.Test
import org.mozilla.fenix.tabstray.NavigationInteractor
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

class SelectionMenuIntegrationTest {

    private val navInteractor = mockk<NavigationInteractor>(relaxed = true)
    private val trayInteractor = mockk<TabsTrayInteractor>(relaxed = true)
    private val capture = CaptureActionsMiddleware<TabsTrayState, TabsTrayAction>()
    private val store = TabsTrayStore(middlewares = listOf(capture))

    @Test
    fun `WHEN bookmark item is clicked THEN invoke interactor and close tray`() {
        val menu = SelectionMenuIntegration(mockk(), store, navInteractor, trayInteractor)

        menu.handleMenuClicked(SelectionMenu.Item.BookmarkTabs)

        store.waitUntilIdle()

        verify { navInteractor.onSaveToBookmarks(store.state.mode.selectedTabs) }

        capture.findLastAction(TabsTrayAction.ExitSelectMode::class)
    }

    @Test
    fun `WHEN delete tabs item is clicked THEN invoke interactor and close tray`() {
        val menu = SelectionMenuIntegration(mockk(), store, navInteractor, trayInteractor)

        menu.handleMenuClicked(SelectionMenu.Item.DeleteTabs)

        store.waitUntilIdle()

        verify { trayInteractor.onDeleteTabs(store.state.mode.selectedTabs) }

        capture.findLastAction(TabsTrayAction.ExitSelectMode::class)
    }
}
