/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import mozilla.components.feature.top.sites.facts.TopSitesFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.log.logger.Logger
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class MetricControllerTest {

    @MockK(relaxUnitFun = true) private lateinit var dataService1: MetricsService
    @MockK(relaxUnitFun = true) private lateinit var dataService2: MetricsService
    @MockK(relaxUnitFun = true) private lateinit var marketingService1: MetricsService
    @MockK(relaxUnitFun = true) private lateinit var marketingService2: MetricsService

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        every { dataService1.type } returns MetricServiceType.Data
        every { dataService2.type } returns MetricServiceType.Data
        every { marketingService1.type } returns MetricServiceType.Marketing
        every { marketingService2.type } returns MetricServiceType.Marketing
    }

    @Test
    fun `debug metric controller emits logs`() {
        val logger = mockk<Logger>(relaxed = true)
        val controller = DebugMetricController(logger)

        controller.start(MetricServiceType.Data)
        verify { logger.debug("DebugMetricController: start") }

        controller.stop(MetricServiceType.Data)
        verify { logger.debug("DebugMetricController: stop") }

        controller.track(Event.OpenedAppFirstRun)
        verify { logger.debug("DebugMetricController: track event: ${Event.OpenedAppFirstRun}") }
    }

    @Test
    fun `release metric controller starts and stops all data services`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1, marketingService1, dataService2, marketingService2),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled }
        )

        controller.start(MetricServiceType.Data)
        verify { dataService1.start() }
        verify { dataService2.start() }

        enabled = false

        controller.stop(MetricServiceType.Data)
        verify { dataService1.stop() }
        verify { dataService2.stop() }

        verifyAll(inverse = true) {
            marketingService1.start()
            marketingService1.stop()
            marketingService2.start()
            marketingService2.stop()
        }
    }

    @Test
    fun `release metric controller starts data service only if enabled`() {
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { false },
            isMarketingDataTelemetryEnabled = { true }
        )

        controller.start(MetricServiceType.Data)
        verify(inverse = true) { dataService1.start() }

        controller.stop(MetricServiceType.Data)
        verify(inverse = true) { dataService1.stop() }
    }

    @Test
    fun `release metric controller starts service only once`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { true }
        )

        controller.start(MetricServiceType.Data)
        controller.start(MetricServiceType.Data)
        verify(exactly = 1) { dataService1.start() }

        enabled = false

        controller.stop(MetricServiceType.Data)
        controller.stop(MetricServiceType.Data)
        verify(exactly = 1) { dataService1.stop() }
    }

    @Test
    fun `release metric controller starts and stops all marketing services`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1, marketingService1, dataService2, marketingService2),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled }
        )

        controller.start(MetricServiceType.Marketing)
        verify { marketingService1.start() }
        verify { marketingService2.start() }

        enabled = false

        controller.stop(MetricServiceType.Marketing)
        verify { marketingService1.stop() }
        verify { marketingService2.stop() }

        verifyAll(inverse = true) {
            dataService1.start()
            dataService1.stop()
            dataService2.start()
            dataService2.stop()
        }
    }

    @Test
    fun `tracking events should be sent to matching service`() {
        val controller = ReleaseMetricController(
            listOf(dataService1, marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true }
        )
        every { dataService1.shouldTrack(Event.TabMediaPause) } returns false
        every { marketingService1.shouldTrack(Event.TabMediaPause) } returns true

        controller.start(MetricServiceType.Marketing)
        controller.track(Event.TabMediaPause)
        verify { marketingService1.track(Event.TabMediaPause) }
    }

    @Test
    fun `tracking events should be sent to enabled service`() {
        var enabled = true
        val controller = ReleaseMetricController(
            listOf(dataService1, marketingService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { true }
        )
        every { dataService1.shouldTrack(Event.TabMediaPause) } returns true
        every { marketingService1.shouldTrack(Event.TabMediaPause) } returns true

        controller.start(MetricServiceType.Marketing)
        enabled = false

        controller.track(Event.TabMediaPause)
        verify { marketingService1.track(Event.TabMediaPause) }
    }

    @Test
    fun `topsites fact should convert to the right events`() {
        var enabled = true
        val controller = ReleaseMetricController(
            services = listOf(dataService1),
            isDataTelemetryEnabled = { enabled },
            isMarketingDataTelemetryEnabled = { enabled }
        )

        var fact = Fact(
            Component.FEATURE_TOP_SITES,
            Action.INTERACTION,
            TopSitesFacts.Items.COUNT,
            "1"
        )

        assertEquals(controller.factToEvent(fact), Event.HaveTopSites)

        fact = Fact(
            Component.FEATURE_TOP_SITES,
            Action.INTERACTION,
            TopSitesFacts.Items.COUNT,
            "0"
        )

        assertEquals(controller.factToEvent(fact), Event.HaveNoTopSites)

        fact = Fact(
            Component.FEATURE_TOP_SITES,
            Action.INTERACTION,
            TopSitesFacts.Items.COUNT,
            "10"
        )

        assertEquals(controller.factToEvent(fact), Event.HaveTopSites)

        fact = Fact(
            Component.FEATURE_TOP_SITES,
            Action.INTERACTION,
            TopSitesFacts.Items.COUNT,
            "-4"
        )

        assertEquals(controller.factToEvent(fact), Event.HaveNoTopSites)

        fact = Fact(
            Component.FEATURE_TOP_SITES,
            Action.INTERACTION,
            TopSitesFacts.Items.COUNT,
            "test"
        )

        assertEquals(controller.factToEvent(fact), Event.HaveNoTopSites)
    }

    @Test
    fun `tracking synced tab event should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true }
        )
        every { marketingService1.shouldTrack(Event.SyncedTabSuggestionClicked) } returns true
        controller.start(MetricServiceType.Marketing)

        controller.track(Event.SyncedTabSuggestionClicked)
        verify { marketingService1.track(Event.SyncedTabSuggestionClicked) }
    }

    @Test
    fun `tracking awesomebar events should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true }
        )
        every { marketingService1.shouldTrack(Event.BookmarkSuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.ClipboardSuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.HistorySuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.SearchActionClicked) } returns true
        every { marketingService1.shouldTrack(Event.SearchSuggestionClicked) } returns true
        every { marketingService1.shouldTrack(Event.OpenedTabSuggestionClicked) } returns true
        controller.start(MetricServiceType.Marketing)

        controller.track(Event.BookmarkSuggestionClicked)
        verify { marketingService1.track(Event.BookmarkSuggestionClicked) }

        controller.track(Event.ClipboardSuggestionClicked)
        verify { marketingService1.track(Event.ClipboardSuggestionClicked) }

        controller.track(Event.HistorySuggestionClicked)
        verify { marketingService1.track(Event.HistorySuggestionClicked) }

        controller.track(Event.SearchActionClicked)
        verify { marketingService1.track(Event.SearchActionClicked) }

        controller.track(Event.SearchSuggestionClicked)
        verify { marketingService1.track(Event.SearchSuggestionClicked) }

        controller.track(Event.OpenedTabSuggestionClicked)
        verify { marketingService1.track(Event.OpenedTabSuggestionClicked) }
    }

    @Test
    fun `tracking bookmark events should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true }
        )
        every { marketingService1.shouldTrack(Event.AddBookmark) } returns true
        every { marketingService1.shouldTrack(Event.RemoveBookmark) } returns true
        every { marketingService1.shouldTrack(Event.OpenedBookmark) } returns true
        every { marketingService1.shouldTrack(Event.OpenedBookmarkInNewTab) } returns true
        every { marketingService1.shouldTrack(Event.OpenedBookmarksInNewTabs) } returns true
        every { marketingService1.shouldTrack(Event.OpenedBookmarkInPrivateTab) } returns true
        every { marketingService1.shouldTrack(Event.OpenedBookmarksInPrivateTabs) } returns true
        every { marketingService1.shouldTrack(Event.EditedBookmark) } returns true
        every { marketingService1.shouldTrack(Event.MovedBookmark) } returns true
        every { marketingService1.shouldTrack(Event.ShareBookmark) } returns true
        every { marketingService1.shouldTrack(Event.CopyBookmark) } returns true
        every { marketingService1.shouldTrack(Event.AddBookmarkFolder) } returns true
        every { marketingService1.shouldTrack(Event.RemoveBookmarkFolder) } returns true
        every { marketingService1.shouldTrack(Event.RemoveBookmarks) } returns true

        controller.start(MetricServiceType.Marketing)

        controller.track(Event.AddBookmark)
        controller.track(Event.RemoveBookmark)
        controller.track(Event.OpenedBookmark)
        controller.track(Event.OpenedBookmarkInNewTab)
        controller.track(Event.OpenedBookmarksInNewTabs)
        controller.track(Event.OpenedBookmarkInPrivateTab)
        controller.track(Event.OpenedBookmarksInPrivateTabs)
        controller.track(Event.EditedBookmark)
        controller.track(Event.MovedBookmark)
        controller.track(Event.ShareBookmark)
        controller.track(Event.CopyBookmark)
        controller.track(Event.AddBookmarkFolder)
        controller.track(Event.RemoveBookmarkFolder)
        controller.track(Event.RemoveBookmarks)

        verify { marketingService1.track(Event.AddBookmark) }
        verify { marketingService1.track(Event.RemoveBookmark) }
        verify { marketingService1.track(Event.OpenedBookmark) }
        verify { marketingService1.track(Event.OpenedBookmarkInNewTab) }
        verify { marketingService1.track(Event.OpenedBookmarksInNewTabs) }
        verify { marketingService1.track(Event.OpenedBookmarkInPrivateTab) }
        verify { marketingService1.track(Event.OpenedBookmarksInPrivateTabs) }
        verify { marketingService1.track(Event.EditedBookmark) }
        verify { marketingService1.track(Event.MovedBookmark) }
        verify { marketingService1.track(Event.ShareBookmark) }
        verify { marketingService1.track(Event.CopyBookmark) }
        verify { marketingService1.track(Event.AddBookmarkFolder) }
        verify { marketingService1.track(Event.RemoveBookmarkFolder) }
        verify { marketingService1.track(Event.RemoveBookmarks) }
    }

    @Test
    fun `history events should be sent to enabled service`() {
        val controller = ReleaseMetricController(
            listOf(marketingService1),
            isDataTelemetryEnabled = { true },
            isMarketingDataTelemetryEnabled = { true }
        )
        every { marketingService1.shouldTrack(Event.HistoryOpenedInNewTab) } returns true
        every { marketingService1.shouldTrack(Event.HistoryOpenedInNewTabs) } returns true
        every { marketingService1.shouldTrack(Event.HistoryOpenedInPrivateTab) } returns true
        every { marketingService1.shouldTrack(Event.HistoryOpenedInPrivateTabs) } returns true

        controller.start(MetricServiceType.Marketing)

        controller.track(Event.HistoryOpenedInNewTab)
        controller.track(Event.HistoryOpenedInNewTabs)
        controller.track(Event.HistoryOpenedInPrivateTab)
        controller.track(Event.HistoryOpenedInPrivateTabs)

        verify { marketingService1.track(Event.HistoryOpenedInNewTab) }
        verify { marketingService1.track(Event.HistoryOpenedInNewTabs) }
        verify { marketingService1.track(Event.HistoryOpenedInPrivateTab) }
        verify { marketingService1.track(Event.HistoryOpenedInPrivateTabs) }
    }
}
