/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Test

class DefaultInactiveTabsInteractorTest {

    @Test
    fun `WHEN onHeaderClicked THEN updateCardExpansion`() {
        val controller: InactiveTabsController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsInteractor(controller)

        interactor.onHeaderClicked(true)

        verify { controller.updateCardExpansion(true) }
    }

    @Test
    fun `WHEN onCloseClicked THEN close`() {
        val controller: InactiveTabsController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsInteractor(controller)

        interactor.onCloseClicked()

        verify { controller.close() }
    }

    @Test
    fun `WHEN onEnabledAutoCloseClicked THEN enableAutoClosed`() {
        val controller: InactiveTabsController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsInteractor(controller)

        interactor.onEnabledAutoCloseClicked()

        verify { controller.enableAutoClosed() }
    }

    @Test
    fun `WHEN an inactive tab is clicked THEN open the tab`() {
        val controller: InactiveTabsController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsInteractor(controller)
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            )
        )

        interactor.onTabClicked(tab)

        verify { controller.openInactiveTab(tab) }
    }

    @Test
    fun `WHEN an inactive tab is clicked to be closed THEN close the tab`() {
        val controller: InactiveTabsController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsInteractor(controller)
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            )
        )

        interactor.onTabClosed(tab)

        verify { controller.closeInactiveTab(tab) }
    }
}
