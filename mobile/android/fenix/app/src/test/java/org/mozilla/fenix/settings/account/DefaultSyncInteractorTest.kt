/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class DefaultSyncInteractorTest {

    private lateinit var syncInteractor: DefaultSyncInteractor
    private lateinit var syncController: DefaultSyncController

    @Before
    fun setUp() {
        syncController = mockk(relaxed = true)
        syncInteractor = DefaultSyncInteractor(syncController)
    }

    @Test
    fun onCameraPermissionsNeeded() {
        syncInteractor.onCameraPermissionsNeeded()

        verify {
            syncController.handleCameraPermissionsNeeded()
        }
    }
}
