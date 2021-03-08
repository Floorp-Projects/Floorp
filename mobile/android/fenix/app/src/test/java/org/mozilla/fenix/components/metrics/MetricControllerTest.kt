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
}
