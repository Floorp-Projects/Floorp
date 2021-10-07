/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test

class DefaultInactiveTabsAutoCloseDialogInteractorTest {

    @Test
    fun `WHEN onCloseClicked THEN close`() {
        val controller: InactiveTabsAutoCloseDialogController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsAutoCloseDialogInteractor(controller)

        interactor.onCloseClicked()

        verify { controller.close() }
    }

    @Test
    fun `WHEN onEnabledAutoCloseClicked THEN enableAutoClosed`() {
        val controller: InactiveTabsAutoCloseDialogController = mockk(relaxed = true)
        val interactor = DefaultInactiveTabsAutoCloseDialogInteractor(controller)

        interactor.onEnabledAutoCloseClicked()

        verify { controller.enableAutoClosed() }
    }
}
