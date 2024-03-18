/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import androidx.appcompat.app.AlertDialog
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity

class DefaultSyncControllerTest {

    private lateinit var syncController: DefaultSyncController

    @MockK(relaxed = true)
    private lateinit var activity: HomeActivity

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        syncController = DefaultSyncController(activity)
    }

    @Test
    fun `show camera permissions needed dialog`() {
        val dialogBuilder: AlertDialog.Builder = mockk(relaxed = true)

        val spyController = spyk(syncController)
        every { spyController.buildDialog() } returns dialogBuilder

        spyController.handleCameraPermissionsNeeded()

        verify { dialogBuilder.show() }
    }
}
