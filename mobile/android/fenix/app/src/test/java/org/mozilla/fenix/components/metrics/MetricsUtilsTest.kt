package org.mozilla.fenix.components.metrics

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.search.SearchEngine
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class MetricsUtilsTest {

    @Test
    fun createSearchEvent() {
        val engine: SearchEngine = mockk(relaxed = true)
        val context = testContext

        every { engine.identifier } returns ENGINE_SOURCE_IDENTIFIER

        assertEquals(
            "$ENGINE_SOURCE_IDENTIFIER.suggestion",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.SUGGESTION
            )?.eventSource?.countLabel
        )
        assertEquals(
            "$ENGINE_SOURCE_IDENTIFIER.action",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.ACTION
            )?.eventSource?.countLabel
        )
        assertEquals(
            "$ENGINE_SOURCE_IDENTIFIER.widget",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.WIDGET
            )?.eventSource?.countLabel
        )
        assertEquals(
            "$ENGINE_SOURCE_IDENTIFIER.shortcut",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.SHORTCUT
            )?.eventSource?.countLabel
        )
    }

    companion object {
        const val ENGINE_SOURCE_IDENTIFIER = "google-2018"
    }
}
