/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.every
import io.mockk.mockk
import io.mockk.verifySequence
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Test

class DefaultTabsTrayInteractorTest {

    val controller: TabsTrayController = mockk(relaxed = true)
    val interactor = DefaultTabsTrayInteractor(controller)

    @Test
    fun `WHEN user selects a new tray page THEN the Interactor delegates to the controller`() {
        interactor.onTrayPositionSelected(14, true)

        verifySequence { controller.handleTrayScrollingToPosition(14, true) }
    }

    @Test
    fun `WHEN user selects a new browser tab THEN the Interactor delegates to the controller`() {
        val tab: TabSessionState = mockk()
        interactor.onTabSelected(tab, null)

        verifySequence { controller.handleTabSelected(tab, null) }
    }

    @Test
    fun `WHEN user deletes one browser tab page THEN the Interactor delegates to the controller`() {
        val tab: TabSessionState = mockk()
        val id = "testTabId"
        every { tab.id } returns id
        interactor.onTabClosed(tab)

        verifySequence { controller.handleTabDeletion(id) }
    }

    @Test
    fun `WHEN user confirms downloads cancellation THEN the Interactor delegates to the controller`() {
        interactor.onDeletePrivateTabWarningAccepted("testTabId")

        verifySequence { controller.handleDeleteTabWarningAccepted("testTabId") }
    }

    @Test
    fun `WHEN user deletes multiple browser tabs THEN the Interactor delegates to the controller`() {
        val tabsToDelete = listOf<TabSessionState>(mockk(), mockk())

        interactor.onDeleteTabs(tabsToDelete)

        verifySequence { controller.handleMultipleTabsDeletion(tabsToDelete) }
    }
}
