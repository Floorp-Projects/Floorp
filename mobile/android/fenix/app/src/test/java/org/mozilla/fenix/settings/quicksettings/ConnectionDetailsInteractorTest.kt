/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ConnectionDetailsInteractorTest {

    private lateinit var controller: ConnectionDetailsController
    private lateinit var interactor: ConnectionDetailsInteractor

    @Before
    fun setUp() {
        controller = mockk(relaxed = true)
        interactor = ConnectionDetailsInteractor(controller)
    }

    @Test
    fun `WHEN onBackPressed is called THEN delegate the controller`() {
        interactor.onBackPressed()

        verify {
            controller.handleBackPressed()
        }
    }
}
