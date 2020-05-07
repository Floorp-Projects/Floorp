/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.search.SearchEngine
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

/**
 * Just the Roboelectric tests for MetricsUtil. Splitting these files out means our other tests will run more quickly.
 * FenixRobolectricTestRunner also breaks our ability to use mockkStatic on Base64.
 */
@RunWith(FenixRobolectricTestRunner::class)
class MetricsUtilsTestRoboelectric {

    @Test
    fun createSearchEvent() {
        val context = testContext
        val engine: SearchEngine = mockk(relaxed = true)

        every { engine.identifier } returns MetricsUtilsTest.ENGINE_SOURCE_IDENTIFIER

        Assert.assertEquals(
            "${MetricsUtilsTest.ENGINE_SOURCE_IDENTIFIER}.suggestion",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.SUGGESTION
            )?.eventSource?.countLabel
        )
        Assert.assertEquals(
            "${MetricsUtilsTest.ENGINE_SOURCE_IDENTIFIER}.action",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.ACTION
            )?.eventSource?.countLabel
        )
        Assert.assertEquals(
            "${MetricsUtilsTest.ENGINE_SOURCE_IDENTIFIER}.widget",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.WIDGET
            )?.eventSource?.countLabel
        )
        Assert.assertEquals(
            "${MetricsUtilsTest.ENGINE_SOURCE_IDENTIFIER}.shortcut",
            MetricsUtils.createSearchEvent(
                engine,
                context,
                Event.PerformedSearch.SearchAccessPoint.SHORTCUT
            )?.eventSource?.countLabel
        )
    }
}
