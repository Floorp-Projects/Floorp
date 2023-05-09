/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.PlayStoreAttribution
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
internal class InstallReferrerMetricsServiceTest {
    val context: Context = ApplicationProvider.getApplicationContext()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun testUtmParamsFromUrl() {
        assertEquals("SOURCE", UTMParams.fromUrl("https://example.com?utm_source=SOURCE").source)
        assertEquals("MEDIUM", UTMParams.fromUrl("https://example.com?utm_medium=MEDIUM").medium)
        assertEquals("CAMPAIGN", UTMParams.fromUrl("https://example.com?utm_campaign=CAMPAIGN").campaign)
        assertEquals("TERM", UTMParams.fromUrl("https://example.com?utm_term=TERM").term)
        assertEquals("CONTENT", UTMParams.fromUrl("https://example.com?utm_content=CONTENT").content)
    }

    @Test
    fun testUtmParamsFromUrlWithSpaces() {
        assertEquals("WITH SPACE", UTMParams.fromUrl("https://example.com?utm_source=WITH+SPACE").source)
        assertEquals("WITH SPACE", UTMParams.fromUrl("https://example.com?utm_medium=WITH%20SPACE").medium)
        assertEquals("WITH SPACE", UTMParams.fromUrl("https://example.com?utm_campaign=WITH SPACE").campaign)
    }

    @Test
    fun testUtmParamsFromUrlWithMissingParams() {
        assertNull(UTMParams.fromUrl("https://example.com?missing=").source)
        assertEquals("", UTMParams.fromUrl("https://example.com?utm_source=").source)
    }

    @Test
    fun testUtmParamsRoundTripThroughSettingsMinimumParams() {
        val settings = Settings(context)
        val expected = UTMParams(source = "", medium = "", campaign = "", content = "", term = "")
        val observed = UTMParams.fromSettings(settings)

        assertEquals(observed, expected)
        assertTrue(observed.isEmpty())
    }

    @Test
    fun testUtmParamsRoundTripThroughSettingsMaximumParams() {
        val expected = UTMParams(source = "source", medium = "medium", campaign = "campaign", content = "content", term = "term")
        val settings = Settings(context)

        expected.intoSettings(settings)
        val observed = UTMParams.fromSettings(settings)

        assertEquals(observed, expected)

        assertFalse(observed.isEmpty())
    }

    @Test
    fun testInstallReferrerMetricsMinimumParams() {
        val service = InstallReferrerMetricsService(context)
        val settings = Settings(context)
        service.recordInstallReferrer(settings, "https://example.com")

        val expected = UTMParams(source = "", medium = "", campaign = "", content = "", term = "")
        val observed = UTMParams.fromSettings(settings)
        assertEquals(observed, expected)

        assertNull(PlayStoreAttribution.source.testGetValue())
        assertNull(PlayStoreAttribution.medium.testGetValue())
        assertNull(PlayStoreAttribution.campaign.testGetValue())
        assertNull(PlayStoreAttribution.content.testGetValue())
        assertNull(PlayStoreAttribution.term.testGetValue())

        assertTrue(observed.isEmpty())
    }

    @Test
    fun testInstallReferrerMetricsPartial() {
        val service = InstallReferrerMetricsService(context)
        val settings = Settings(context)
        service.recordInstallReferrer(settings, "https://example.com?utm_campaign=CAMPAIGN")

        val expected = UTMParams(source = "", medium = "", campaign = "CAMPAIGN", content = "", term = "")
        val observed = UTMParams.fromSettings(settings)
        assertEquals(observed, expected)

        assertNull(PlayStoreAttribution.source.testGetValue())
        assertNull(PlayStoreAttribution.medium.testGetValue())
        assertEquals("CAMPAIGN", PlayStoreAttribution.campaign.testGetValue())
        assertNull(PlayStoreAttribution.content.testGetValue())
        assertNull(PlayStoreAttribution.term.testGetValue())

        assertFalse(observed.isEmpty())
    }

    @Test
    fun testInstallReferrerMetricsMaximumParams() {
        val service = InstallReferrerMetricsService(context)
        val settings = Settings(context)
        service.recordInstallReferrer(settings, "https://example.com?utm_source=SOURCE&utm_medium=MEDIUM&utm_campaign=CAMPAIGN&utm_content=CONTENT&utm_term=TERM")

        val expected = UTMParams(source = "SOURCE", medium = "MEDIUM", campaign = "CAMPAIGN", content = "CONTENT", term = "TERM")
        val observed = UTMParams.fromSettings(settings)
        assertEquals(observed, expected)

        assertEquals("SOURCE", PlayStoreAttribution.source.testGetValue())
        assertEquals("MEDIUM", PlayStoreAttribution.medium.testGetValue())
        assertEquals("CAMPAIGN", PlayStoreAttribution.campaign.testGetValue())
        assertEquals("CONTENT", PlayStoreAttribution.content.testGetValue())
        assertEquals("TERM", PlayStoreAttribution.term.testGetValue())

        assertFalse(observed.isEmpty())
    }

    @Test
    fun testInstallReferrerMetricsShouldTrack() {
        val service = InstallReferrerMetricsService(context)
        assertFalse(service.shouldTrack(Event.GrowthData.FirstAppOpenForDay))
    }

    @Test
    fun testInstallReferrerMetricsType() {
        val service = InstallReferrerMetricsService(context)
        assertEquals(MetricServiceType.Marketing, service.type)
    }
}
