/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.view.LayoutInflater
import androidx.lifecycle.LifecycleOwner
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.databinding.TopSiteItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor

@RunWith(FenixRobolectricTestRunner::class)
class TopSiteItemViewHolderTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var binding: TopSiteItemBinding
    private lateinit var interactor: TopSiteInteractor
    private lateinit var lifecycleOwner: LifecycleOwner
    private lateinit var metrics: MetricController

    private val pocket = TopSite.Default(
        id = 1L,
        title = "Pocket",
        url = "https://getpocket.com",
        createdAt = 0
    )

    @Before
    fun setup() {
        binding = TopSiteItemBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
        lifecycleOwner = mockk(relaxed = true)
        metrics = mockk(relaxed = true)

        every { testContext.components.core.icons } returns BrowserIcons(testContext, mockk(relaxed = true))
        every { testContext.components.analytics.metrics } returns metrics
    }

    @Test
    fun `calls interactor on click`() {
        TopSiteItemViewHolder(binding.root, lifecycleOwner, interactor).bind(pocket, position = 0)

        binding.topSiteItem.performClick()
        verify { interactor.onSelectTopSite(pocket, position = 0) }
    }

    @Test
    fun `calls interactor on long click`() {
        every { testContext.components.analytics } returns mockk(relaxed = true)
        TopSiteItemViewHolder(binding.root, lifecycleOwner, interactor).bind(pocket, position = 0)

        binding.topSiteItem.performLongClick()
        verify { interactor.onTopSiteMenuOpened() }
    }

    @Test
    fun `GIVEN a default top site WHEN bind is called THEN the title has a pin indicator`() {
        val defaultTopSite = TopSite.Default(
            id = 1L,
            title = "Pocket",
            url = "https://getpocket.com",
            createdAt = 0
        )

        TopSiteItemViewHolder(binding.root, lifecycleOwner, interactor).bind(defaultTopSite, position = 0)
        val pinIndicator = binding.topSiteTitle.compoundDrawables[0]

        assertNotNull(pinIndicator)
    }

    @Test
    fun `GIVEN a pinned top site WHEN bind is called THEN the title has a pin indicator`() {
        val pinnedTopSite = TopSite.Pinned(
            id = 1L,
            title = "Mozilla",
            url = "https://www.mozilla.org",
            createdAt = 0
        )

        TopSiteItemViewHolder(binding.root, lifecycleOwner, interactor).bind(pinnedTopSite, position = 0)
        val pinIndicator = binding.topSiteTitle.compoundDrawables[0]

        assertNotNull(pinIndicator)
    }

    @Test
    fun `GIVEN a frecent top site WHEN bind is called THEN the title does not have a pin indicator`() {
        val frecentTopSite = TopSite.Frecent(
            id = 1L,
            title = "Mozilla",
            url = "https://www.mozilla.org",
            createdAt = 0
        )

        TopSiteItemViewHolder(binding.root, lifecycleOwner, interactor).bind(frecentTopSite, position = 0)
        val pinIndicator = binding.topSiteTitle.compoundDrawables[0]

        assertNull(pinIndicator)
    }

    @Test
    fun `GIVEN a provided top site and position WHEN the provided top site is shown THEN submit a top site impression ping`() {
        val topSite = TopSite.Provided(
            id = 3,
            title = "Mozilla",
            url = "https://mozilla.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3
        )
        val position = 0
        assertFalse(TopSites.contileImpression.testHasValue())

        var topSiteImpressionSubmitted = false
        Pings.topsitesImpression.testBeforeNextSubmit {
            assertTrue(TopSites.contileTileId.testHasValue())
            assertEquals(3, TopSites.contileTileId.testGetValue())

            assertTrue(TopSites.contileAdvertiser.testHasValue())
            assertEquals("mozilla", TopSites.contileAdvertiser.testGetValue())

            assertTrue(TopSites.contileReportingUrl.testHasValue())
            assertEquals(topSite.impressionUrl, TopSites.contileReportingUrl.testGetValue())

            topSiteImpressionSubmitted = true
        }

        TopSiteItemViewHolder(binding.root, lifecycleOwner, interactor).submitTopSitesImpressionPing(topSite, position)

        assertTrue(TopSites.contileImpression.testHasValue())

        val event = TopSites.contileImpression.testGetValue()

        assertEquals(1, event.size)
        assertEquals("top_sites", event[0].category)
        assertEquals("contile_impression", event[0].name)
        assertEquals("1", event[0].extra!!["position"])
        assertEquals("newtab", event[0].extra!!["source"])

        assertTrue(topSiteImpressionSubmitted)
    }
}
