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
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
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
        assertNull(Metrics.searchCount["custom.action"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.ACTION,
        )

        assertNotNull(Metrics.searchCount["custom.action"].testGetValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a SHORTCUT the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["custom.shortcut"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SHORTCUT,
        )

        assertNotNull(Metrics.searchCount["custom.shortcut"].testGetValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a SUGGESTION the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["custom.suggestion"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SUGGESTION,
        )

        assertNotNull(Metrics.searchCount["custom.suggestion"].testGetValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a TOPSITE the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["custom.topsite"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.TOPSITE,
        )

        assertNotNull(Metrics.searchCount["custom.topsite"].testGetValue())
    }

    @Test
    fun `given a CUSTOM engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["custom.widget"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.CUSTOM

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET,
        )

        assertNotNull(Metrics.searchCount["custom.widget"].testGetValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is an ACTION the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.action"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.ACTION,
        )

        assertNotNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.action"].testGetValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a TOPSITE the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.topsite"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.TOPSITE,
        )

        assertNotNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.topsite"].testGetValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a SHORTCUT the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.shortcut"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SHORTCUT,
        )

        assertNotNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.shortcut"].testGetValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a SUGGESTION the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.suggestion"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.SUGGESTION,
        )

        assertNotNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.suggestion"].testGetValue())
    }

    @Test
    fun `given a BUNDLED engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.widget"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET,
        )

        assertNotNull(Metrics.searchCount["$ENGINE_SOURCE_IDENTIFIER.widget"].testGetValue())
    }

    @Test
    fun `given a BUNDLED engine with an uppercase id, when recording a new search with that engine then record using lowercase`() {
        val searchEngineId = "Uppercase-Id"
        assertNull(Metrics.searchCount["$searchEngineId.widget"].testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns searchEngineId
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET,
        )

        assertNotNull(Metrics.searchCount["${searchEngineId.lowercase()}.widget"].testGetValue())
    }

    @Test
    fun `given a DEFAULT engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertNull(Events.performedSearch.testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            true,
            MetricsUtils.Source.WIDGET,
        )

        assertNotNull(Events.performedSearch.testGetValue())
        val snapshot = Events.performedSearch.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("default.widget", snapshot.single().extra?.getValue("source"))
    }

    @Test
    fun `given a NON DEFAULT engine, when the search source is a WIDGET the proper labeled metric is recorded`() {
        assertNull(Events.performedSearch.testGetValue())

        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.id } returns ENGINE_SOURCE_IDENTIFIER
        every { engine.type } returns SearchEngine.Type.BUNDLED

        MetricsUtils.recordSearchMetrics(
            engine,
            false,
            MetricsUtils.Source.WIDGET,
        )

        assertNotNull(Events.performedSearch.testGetValue())
        val snapshot = Events.performedSearch.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("shortcut.widget", snapshot.single().extra?.getValue("source"))
    }
}
