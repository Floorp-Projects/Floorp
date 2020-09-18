/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.SessionUseCases
import org.junit.Test

class TabHistoryControllerTest {

    private val store: BrowserStore = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val sessionUseCases = SessionUseCases(store, sessionManager)

    private val goToHistoryIndexUseCase = sessionUseCases.goToHistoryIndex
    private val controller = DefaultTabHistoryController(
        navController = navController,
        goToHistoryIndexUseCase = goToHistoryIndexUseCase,
        sessionManager = sessionManager
    )

    private val currentItem = TabHistoryItem(
        index = 0,
        title = "",
        url = "",
        isSelected = true
    )

    @Test
    fun handleGoToHistoryIndexNormalBrowsing() {
        val session: Session = mockk(relaxed = true)
        every { sessionManager.selectedSession } returns session

        controller.handleGoToHistoryItem(currentItem)

        verify { navController.navigateUp() }
        verify { goToHistoryIndexUseCase.invoke(currentItem.index, session) }
    }

    @Test
    fun handleGoToHistoryIndexCustomTab() {
        val customTabController = DefaultTabHistoryController(
            navController = navController,
            goToHistoryIndexUseCase = goToHistoryIndexUseCase,
            customTabId = "custom-id",
            sessionManager = sessionManager
        )
        val customTabSession: Session = mockk(relaxed = true)

        every { sessionManager.findSessionById(any()) } returns customTabSession

        customTabController.handleGoToHistoryItem(currentItem)

        verify { navController.navigateUp() }
        verify { goToHistoryIndexUseCase.invoke(currentItem.index, customTabSession) }
    }
}
