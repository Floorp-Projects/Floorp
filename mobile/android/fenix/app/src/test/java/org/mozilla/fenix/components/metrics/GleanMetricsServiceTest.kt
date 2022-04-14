/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.RecentlyVisitedHomepage
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class GleanMetricsServiceTest {
    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    private lateinit var gleanService: GleanMetricsService

    @Before
    fun setup() {
        gleanService = GleanMetricsService(testContext)
    }

    @Test
    fun `Home screen recently visited events are correctly recorded`() {
        assertFalse(RecentlyVisitedHomepage.historyHighlightOpened.testHasValue())
        gleanService.track(Event.HistoryHighlightOpened)
        assertTrue(RecentlyVisitedHomepage.historyHighlightOpened.testHasValue())

        assertFalse(RecentlyVisitedHomepage.searchGroupOpened.testHasValue())
        gleanService.track(Event.HistorySearchGroupOpened)
        assertTrue(RecentlyVisitedHomepage.searchGroupOpened.testHasValue())
    }
}
