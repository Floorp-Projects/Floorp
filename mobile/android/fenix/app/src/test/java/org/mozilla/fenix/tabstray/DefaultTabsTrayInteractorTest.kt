/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.mockk
import io.mockk.verifySequence
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Test

class DefaultTabsTrayInteractorTest {

    val controller: TabsTrayController = mockk(relaxed = true)
    val trayInteractor = DefaultTabsTrayInteractor(controller)

    @Test
    fun `GIVEN user selecting a new tray page WHEN onTrayPositionSelected is called THEN the Interactor delegates the controller`() {
        trayInteractor.onTrayPositionSelected(14, true)

        verifySequence { controller.handleTrayScrollingToPosition(14, true) }
    }

    @Test
    fun `GIVEN user selecting a new browser tab WHEN onBrowserTabSelected is called THEN the Interactor delegates the controller`() {
        trayInteractor.onBrowserTabSelected()

        verifySequence { controller.handleNavigateToBrowser() }
    }

    @Test
    fun `GIVEN user deleted one browser tab page WHEN onDeleteTab is called THEN the Interactor delegates the controller`() {
        trayInteractor.onDeleteTab("testTabId")

        verifySequence { controller.handleTabDeletion("testTabId") }
    }

    @Test
    fun `GIVEN user confirmed downloads cancellation WHEN onDeletePrivateTabWarningAccepted is called THEN the Interactor delegates the controller`() {
        trayInteractor.onDeletePrivateTabWarningAccepted("testTabId")

        verifySequence { controller.handleDeleteTabWarningAccepted("testTabId") }
    }

    @Test
    fun `GIVEN user deleted multiple browser tabs WHEN onDeleteTabs is called THEN the Interactor delegates the controller`() {
        val tabsToDelete = listOf<TabSessionState>(mockk(), mockk())

        trayInteractor.onDeleteTabs(tabsToDelete)

        verifySequence { controller.handleMultipleTabsDeletion(tabsToDelete) }
    }
}
