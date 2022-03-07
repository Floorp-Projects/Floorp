/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class RemoveTabUseCaseWrapperTest {

    val metricController = mockk<MetricController>(relaxed = true)

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `WHEN invoked with no source name THEN metrics with unknown source, use case and callback are triggered`() {
        var actualTabId: String? = null
        val onRemove: (String) -> Unit = { tabId ->
            actualTabId = tabId
        }
        val wrapper = RemoveTabUseCaseWrapper(onRemove)

        assertFalse(TabsTray.closedExistingTab.testHasValue())

        wrapper("123")

        assertTrue(TabsTray.closedExistingTab.testHasValue())
        val snapshot = TabsTray.closedExistingTab.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("unknown", snapshot.single().extra?.getValue("source"))
        assertEquals("123", actualTabId)
    }

    @Test
    fun `WHEN invoked with a source name THEN metrics containing the source, use case and callback are triggered`() {
        var actualTabId: String? = null
        val onRemove: (String) -> Unit = { tabId ->
            actualTabId = tabId
        }
        val wrapper = RemoveTabUseCaseWrapper(onRemove)

        assertFalse(TabsTray.closedExistingTab.testHasValue())

        wrapper("123", "Test")

        assertTrue(TabsTray.closedExistingTab.testHasValue())
        val snapshot = TabsTray.closedExistingTab.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("Test", snapshot.single().extra?.getValue("source"))
        assertEquals("123", actualTabId)
    }
}
