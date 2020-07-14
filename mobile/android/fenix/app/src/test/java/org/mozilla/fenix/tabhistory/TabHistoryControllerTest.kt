/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases
import org.junit.Test

class TabHistoryControllerTest {

    val sessionManager: SessionManager = mockk(relaxed = true)
    val navController: NavController = mockk(relaxed = true)
    val sessionUseCases = SessionUseCases(sessionManager)
    val goToHistoryIndexUseCase = sessionUseCases.goToHistoryIndex
    val controller = DefaultTabHistoryController(
        navController = navController,
        goToHistoryIndexUseCase = goToHistoryIndexUseCase
    )

    val currentItem = TabHistoryItem(
        index = 0,
        title = "",
        url = "",
        isSelected = true
    )

    @Test
    fun handleGoToHistoryIndex() {
        controller.handleGoToHistoryItem(currentItem)

        verify { goToHistoryIndexUseCase.invoke(currentItem.index) }
    }
}
