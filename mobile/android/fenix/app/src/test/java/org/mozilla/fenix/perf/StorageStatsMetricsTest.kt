/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.usage.StorageStats
import android.app.usage.StorageStatsManager
import android.content.Context
import androidx.core.content.getSystemService
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.GleanMetrics.StorageStats as Metrics

@RunWith(FenixRobolectricTestRunner::class) // gleanTestRule
class StorageStatsMetricsTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @RelaxedMockK private lateinit var mockContext: Context
    @RelaxedMockK private lateinit var storageStats: StorageStats

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        every {
            mockContext.getSystemService<StorageStatsManager>()?.queryStatsForUid(any(), any())
        } returns storageStats
    }

    @Test
    fun `WHEN reporting THEN the values from the storageStats are accumulated`() {
        every { storageStats.appBytes } returns 100
        every { storageStats.cacheBytes } returns 200
        every { storageStats.dataBytes } returns 1000

        StorageStatsMetrics.reportSync(mockContext)

        assertEquals(100, Metrics.appBytes.testGetValue().sum)
        assertEquals(200, Metrics.cacheBytes.testGetValue().sum)
        assertEquals(800, Metrics.dataDirBytes.testGetValue().sum)
    }

    @Test
    fun `WHEN reporting THEN the query duration is measured`() {
        StorageStatsMetrics.reportSync(mockContext)
        assertTrue(Metrics.queryStatsDuration.testHasValue())
    }
}
