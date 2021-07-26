/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class RemoveTabUseCaseWrapperTest {

    val metricController = mockk<MetricController>(relaxed = true)

    @Test
    fun `WHEN invoked with no source name THEN metrics with unknown source, use case and callback are triggered`() {
        var actualTabId: String? = null
        val onRemove: (String) -> Unit = { tabId ->
            actualTabId = tabId
        }
        val wrapper = RemoveTabUseCaseWrapper(metricController, onRemove)

        wrapper("123")

        verify { metricController.track(Event.ClosedExistingTab("unknown")) }
        assertEquals("123", actualTabId)
    }

    @Test
    fun `WHEN invoked with a source name THEN metrics containing the source, use case and callback are triggered`() {
        var actualTabId: String? = null
        val onRemove: (String) -> Unit = { tabId ->
            actualTabId = tabId
        }
        val wrapper = RemoveTabUseCaseWrapper(metricController, onRemove)

        wrapper("123", "Test")

        verify { metricController.track(Event.ClosedExistingTab("Test")) }
        assertEquals("123", actualTabId)
    }
}
