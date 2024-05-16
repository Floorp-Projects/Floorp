/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.toolbar.navbar.BottomToolbarContainerIntegration
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class BottomToolbarContainerIntegrationTest {
    private lateinit var feature: BottomToolbarContainerIntegration

    @Before
    fun setup() {
        feature = BottomToolbarContainerIntegration(
            toolbar = mockk(),
            store = mockk(),
            appStore = mockk(),
            bottomToolbarContainerView = mockk(),
            sessionId = null,
        ).apply {
            toolbarController = mockk(relaxed = true)
        }
    }

    @Test
    fun `WHEN the feature starts THEN toolbar controllers starts as well`() {
        feature.start()

        verify { feature.toolbarController.start() }
    }

    @Test
    fun `WHEN the feature stops THEN toolbar controllers stops as well`() {
        feature.stop()

        verify { feature.toolbarController.stop() }
    }
}
