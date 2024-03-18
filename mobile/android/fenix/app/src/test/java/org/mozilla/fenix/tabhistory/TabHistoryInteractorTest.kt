/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test

class TabHistoryInteractorTest {

    val controller: TabHistoryController = mockk(relaxed = true)
    val interactor = TabHistoryInteractor(controller)

    @Test
    fun onGoToHistoryItem() {
        val item: TabHistoryItem = mockk()

        interactor.goToHistoryItem(item)

        verify { controller.handleGoToHistoryItem(item) }
    }
}
