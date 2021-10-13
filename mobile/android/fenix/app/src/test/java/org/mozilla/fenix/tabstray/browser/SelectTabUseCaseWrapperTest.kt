/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class SelectTabUseCaseWrapperTest {

    val metricController = mockk<MetricController>(relaxed = true)
    val selectUseCase: TabsUseCases.SelectTabUseCase = mockk(relaxed = true)

    @Test
    fun `WHEN invoked with no source name THEN metrics with unknown source, use case and callback are triggered`() {
        var invoked = ""
        val onSelect: (String) -> Unit = { invoked = it }
        val wrapper = SelectTabUseCaseWrapper(metricController, selectUseCase, onSelect)

        wrapper("123")

        verify { metricController.track(Event.OpenedExistingTab("unknown")) }
        verify { selectUseCase("123") }
        assertEquals("123", invoked)
    }

    @Test
    fun `WHEN invoked with a source name THEN metrics, use case and callback are triggered`() {
        var invoked = ""
        val onSelect: (String) -> Unit = { invoked = it }
        val wrapper = SelectTabUseCaseWrapper(metricController, selectUseCase, onSelect)

        wrapper("123", "Test")

        verify { metricController.track(Event.OpenedExistingTab("Test")) }
        verify { selectUseCase("123") }
        assertEquals("123", invoked)
    }
}
