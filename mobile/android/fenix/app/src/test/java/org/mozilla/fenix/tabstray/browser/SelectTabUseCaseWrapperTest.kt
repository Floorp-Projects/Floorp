/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.tabs.TabsUseCases
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
class SelectTabUseCaseWrapperTest {

    val metricController = mockk<MetricController>(relaxed = true)
    val selectUseCase: TabsUseCases.SelectTabUseCase = mockk(relaxed = true)

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `WHEN invoked with no source name THEN metrics with unknown source, use case and callback are triggered`() {
        var invoked = ""
        val onSelect: (String) -> Unit = { invoked = it }
        val wrapper = SelectTabUseCaseWrapper(selectUseCase, onSelect)

        assertFalse(TabsTray.openedExistingTab.testHasValue())

        wrapper("123")

        assertTrue(TabsTray.openedExistingTab.testHasValue())
        val snapshot = TabsTray.openedExistingTab.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("unknown", snapshot.single().extra?.getValue("source"))

        verify { selectUseCase("123") }
        assertEquals("123", invoked)
    }

    @Test
    fun `WHEN invoked with a source name THEN metrics, use case and callback are triggered`() {
        var invoked = ""
        val onSelect: (String) -> Unit = { invoked = it }
        val wrapper = SelectTabUseCaseWrapper(selectUseCase, onSelect)

        assertFalse(TabsTray.openedExistingTab.testHasValue())

        wrapper("123", "Test")

        assertTrue(TabsTray.openedExistingTab.testHasValue())
        val snapshot = TabsTray.openedExistingTab.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("Test", snapshot.single().extra?.getValue("source"))

        verify { selectUseCase("123") }
        assertEquals("123", invoked)
    }
}
