/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import androidx.navigation.NavController
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.session.SessionUseCases
import org.junit.Before
import org.junit.Test

class TabHistoryControllerTest {

    private lateinit var navController: NavController
    private lateinit var goToHistoryIndexUseCase: SessionUseCases.GoToHistoryIndexUseCase
    private lateinit var currentItem: TabHistoryItem

    @Before
    fun setUp() {
        navController = mockk(relaxed = true)
        goToHistoryIndexUseCase = mockk(relaxed = true)
        currentItem = TabHistoryItem(
            index = 0,
            title = "",
            url = "",
            isSelected = true
        )
    }

    @Test
    fun handleGoToHistoryIndexNormalBrowsing() {
        val controller = DefaultTabHistoryController(
            navController = navController,
            goToHistoryIndexUseCase = goToHistoryIndexUseCase
        )

        controller.handleGoToHistoryItem(currentItem)
        verify { navController.navigateUp() }
        verify { goToHistoryIndexUseCase.invoke(currentItem.index) }
    }

    @Test
    fun handleGoToHistoryIndexCustomTab() {
        val customTabId = "customTabId"
        val customTabController = DefaultTabHistoryController(
            navController = navController,
            goToHistoryIndexUseCase = goToHistoryIndexUseCase,
            customTabId = customTabId
        )

        customTabController.handleGoToHistoryItem(currentItem)
        verify { navController.navigateUp() }
        verify { goToHistoryIndexUseCase.invoke(currentItem.index, customTabId) }
    }
}
