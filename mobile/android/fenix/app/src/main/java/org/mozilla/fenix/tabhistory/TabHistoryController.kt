/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import androidx.navigation.NavController
import mozilla.components.feature.session.SessionUseCases

interface TabHistoryController {
    fun handleGoToHistoryItem(item: TabHistoryItem)
}

class DefaultTabHistoryController(
    private val navController: NavController,
    private val goToHistoryIndexUseCase: SessionUseCases.GoToHistoryIndexUseCase,
    private val customTabId: String? = null,
) : TabHistoryController {

    override fun handleGoToHistoryItem(item: TabHistoryItem) {
        navController.navigateUp()

        if (customTabId != null) {
            goToHistoryIndexUseCase.invoke(item.index, customTabId)
        } else {
            goToHistoryIndexUseCase.invoke(item.index)
        }
    }
}
