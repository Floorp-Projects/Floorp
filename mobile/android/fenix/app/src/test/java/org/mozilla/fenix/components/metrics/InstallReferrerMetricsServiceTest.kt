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
import org.junit.Assert.assertNotNull
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
        assertEquals("SOURCE", UTMParams.fromQueryString("utm_source=SOURCE").source)
        assertEquals("MEDIUM", UTMParams.fromQueryString("utm_medium=MEDIUM").medium)
        assertEquals("CAMPAIGN", UTMParams.fromQueryString("utm_campaign=CAMPAIGN").campaign)
        assertEquals("TERM", UTMParams.fromQueryString("utm_term=TERM").term)
        assertEquals("CONTENT", UTMParams.fromQueryString("utm_content=CONTENT").content)
    }

    @Test
    fun testUtmParamsFromUrlWithSpaces() {
        assertEquals("WITH SPACE", UTMParams.fromQueryString("utm_source=WITH+SPACE").source)
        assertEquals("WITH SPACE", UTMParams.fromQueryString("utm_medium=WITH%20SPACE").medium)
        assertEquals("WITH SPACE", UTMParams.fromQueryString("utm_campaign=WITH SPACE").campaign)
    }

    @Test
    fun testUtmParamsFromUrlWithMissingParams() {
        assertNull(UTMParams.fromQueryString("missing=").source)
        assertEquals("", UTMParams.fromQueryString("utm_source=").source)
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
        assertEquals(expected, observed)

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

    @Test
    fun testDecodeReferrerUrl() {
        // Example from https://developers.google.com/analytics/devguides/collection/android/v4/campaigns#google-play-url-builder
        val params = UTMParams.fromURLString(
            "https://play.google.com/store/apps/details?id=com.example.application" +
                "&referrer=utm_source%3Dgoogle" +
                "%26utm_medium%3Dcpc" +
                "%26utm_term%3Drunning%252Bshoes" +
                "%26utm_content%3Dlogolink" +
                "%26utm_campaign%3Dspring_sale",
        )
        assertNotNull(params)

        val expected = UTMParams(
            source = "google",
            medium = "cpc",
            campaign = "spring_sale",
            content = "logolink",
            term = "running+shoes",
        )

        assertEquals(expected, params)
    }

    @Test
    fun testDecodeReferrerAdMobUrl() {
        val expected = UTMParams(source = "SOURCE", medium = "MEDIUM", campaign = "CAMPAIGN", content = "CONTENT", term = "TERM")
        // Generated with https://developers.google.com/analytics/devguides/collection/android/v4/campaigns#google-play-url-builder
        val fromUrl = UTMParams.fromURLString(
            "https://play.google.com/store/apps/details?id=org.mozilla.fenix&referrer=utm_source%3DSOURCE%26utm_medium%3DMEDIUM%26utm_term%3DTERM%26utm_content%3DCONTENT%26utm_campaign%3DCAMPAIGN%26anid%3Dadmob",
        )
        assertNotNull(fromUrl)
        assertEquals(expected, fromUrl)

        val fromReferrerAttribute = UTMParams.fromURLString("utm_source%3DSOURCE%26utm_medium%3DMEDIUM%26utm_term%3DTERM%26utm_content%3DCONTENT%26utm_campaign%3DCAMPAIGN%26anid%3Dadmob")
        assertNotNull(fromReferrerAttribute)
        assertEquals(expected, fromReferrerAttribute)
    }
}
