/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.components.metrics.MetricsUtilsTest.Companion.ENGINE_SOURCE_IDENTIFIER
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

/**
 * Just the Robolectric tests for MetricsUtil. Splitting these files out means our other tests will run more quickly.
 * FenixRobolectricTestRunner also breaks our ability to use mockkStatic on Base64.
 */
@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class MetricsUtilsTestRobolectric {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `given a CUSTOM engine, when the search source is a ACTION the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["custom.action"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.ACTION
        )

        assertTrue(Metrics.searchCount["custom.action"].testHasValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a SHORTCUT the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["custom.shortcut"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SHORTCUT
        )

        assertTrue(Metrics.searchCount["custom.shortcut"].testHasValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a SUGGESTION the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["custom.suggestion"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SUGGESTION
        )

        assertTrue(Metrics.searchCount["custom.suggestion"].testHasValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a TOPSITE the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["custom.topsite"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.TOPSITE
        )

        assertTrue(Metrics.searchCount["custom.topsite"].testHasValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["custom.widget"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET
        )

        assertTrue(Metrics.searchCount["custom.widget"].testHasValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is an ACTION the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.action"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.ACTION
        )

        assertTrue(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.action"].testHasValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a TOPSITE the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.topsite"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.TOPSITE
        )

        assertTrue(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.topsite"].testHasValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a SHORTCUT the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.shortcut"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SHORTCUT
        )

        assertTrue(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.shortcut"].testHasValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a SUGGESTION the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.suggestion"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SUGGESTION
        )

        assertTrue(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.suggestion"].testHasValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertFalse(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.widget"].testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET
        )

        assertTrue(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.widget"].testHasValue())
    }

    @Test
    fun `given a DEFAULT engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertFalse(Events.performedSearch.testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            true,
            MetricsUtils.Source.WIDGET
        )

        assertTrue(Events.performedSearch.testHasValue())
        val snapshot = Events.performedSearch.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("default.widget", snapshot.single().extra?.getValue("source"))
    }

    @Test
    fun `given a NON DEFAULT engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertFalse(Events.performedSearch.testHasValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET
        )

        assertTrue(Events.performedSearch.testHasValue())
        val snapshot = Events.performedSearch.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("shortcut.widget", snapshot.single().extra?.getValue("source"))
    }
}
