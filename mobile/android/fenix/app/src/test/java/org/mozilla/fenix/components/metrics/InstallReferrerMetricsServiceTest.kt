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
import org.mozilla.fenix.GleanMetrics.MetaAttribution
import org.mozilla.fenix.GleanMetrics.PlayStoreAttribution
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
internal class InstallReferrerMetricsServiceTest {
    val context: Context = ApplicationProvider.getApplicationContext()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `WHEN retrieving minimum UTM params from setting THEN result should match`() {
        val settings = Settings(context)
        val expected = UTMParams(source = "", medium = "", campaign = "", content = "", term = "")
        val observed = UTMParams.fromSettings(settings)

        assertEquals(observed, expected)
        assertTrue(observed.isEmpty())
    }

    @Test
    fun `WHEN retrieving maximum UTM params from setting THEN result should match`() {
        val expected = UTMParams(source = "source", medium = "medium", campaign = "campaign", content = "content", term = "term")
        val settings = Settings(context)

        expected.intoSettings(settings)
        val observed = UTMParams.fromSettings(settings)

        assertEquals(observed, expected)

        assertFalse(observed.isEmpty())
    }

    @Test
    fun `WHEN parsing referrer response with no UTM params from setting THEN UTM params in settings should set to empty strings`() {
        val settings = Settings(context)
        val params = UTMParams.parseUTMParameters("")
        params.recordInstallReferrer(settings)

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
    fun `WHEN parsing referrer response with partial UTM params from setting THEN UTM params in settings should match expected`() {
        val settings = Settings(context)
        val params = UTMParams.parseUTMParameters("utm_campaign=CAMPAIGN")
        params.recordInstallReferrer(settings)

        val expected = UTMParams(source = "", medium = "", campaign = "CAMPAIGN", content = "", term = "")
        val observed = UTMParams.fromSettings(settings)
        assertEquals(observed, expected)

        assertEquals("", PlayStoreAttribution.source.testGetValue())
        assertEquals("", PlayStoreAttribution.medium.testGetValue())
        assertEquals("CAMPAIGN", PlayStoreAttribution.campaign.testGetValue())
        assertEquals("", PlayStoreAttribution.content.testGetValue())
        assertEquals("", PlayStoreAttribution.term.testGetValue())

        assertFalse(observed.isEmpty())
    }

    @Test
    fun `WHEN parsing referrer response with full UTM params from setting THEN UTM params in settings should match expected`() {
        val settings = Settings(context)
        val params = UTMParams.parseUTMParameters("utm_source=SOURCE&utm_medium=MEDIUM&utm_campaign=CAMPAIGN&utm_content=CONTENT&utm_term=TERM")
        params.recordInstallReferrer(settings)

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
    fun `WHEN Install referrer metrics service should track is called THEN it should always return false`() {
        val service = InstallReferrerMetricsService(context)
        assertFalse(service.shouldTrack(Event.GrowthData.FirstAppOpenForDay))
    }

    @Test
    fun `WHEN Install referrer metrics service starts THEN then the service type should be marketing`() {
        val service = InstallReferrerMetricsService(context)
        assertEquals(MetricServiceType.Marketing, service.type)
    }

    @Test
    fun `WHEN receiving a Meta encrypted attribution THEN will decrypt correctly`() {
        val metaParams = MetaParams.extractMetaAttribution("""{"app":12345, "t":1234567890,"source":{"data":"DATA","nonce":"NONCE"}}""")
        val expectedMetaParams = MetaParams("12345", "1234567890", "DATA", "NONCE")

        assertEquals(metaParams, expectedMetaParams)
    }

    @Test
    fun `WHEN parsing referrer response with meta attribution THEN both UTM and Meta params should match expected`() {
        val utmParams = UTMParams.parseUTMParameters("""utm_content={"app":12345, "t":1234567890,"source":{"data":"DATA","nonce":"NONCE"}}""")
        val expectedUtmParams = UTMParams(source = "", medium = "", campaign = "", content = """{"app":12345, "t":1234567890,"source":{"data":"DATA","nonce":"NONCE"}}""", term = "")

        assertEquals(utmParams, expectedUtmParams)

        val metaParams = MetaParams.extractMetaAttribution(utmParams.content)
        val expectedMetaParams = MetaParams("12345", "1234567890", "DATA", "NONCE")

        assertEquals(metaParams, expectedMetaParams)
    }

    @Test
    fun `WHEN recording Meta attribution THEN correct values should be recorded to telemetry`() {
        // The data and nonce are from Meta's example https://developers.facebook.com/docs/app-ads/install-referrer/
        val metaParams = MetaParams(
            "12345",
            "1234567890",
            "afe56cf6228c6ea8c79da49186e718e92a579824596ae1d0d4d20d7793dca797bd4034ccf467bfae5c79a3981e7a2968c41949237e2b2db678c1c3d39c9ae564c5cafd52f2b77a3dc77bf1bae063114d0283b97417487207735da31ddc1531d5645a9c3e602c195a0ebf69c272aa5fda3a2d781cb47e117310164715a54c7a5a032740584e2789a7b4e596034c16425139a77e507c492b629c848573c714a03a2e7d25b9459b95842332b460f3682d19c35dbc7d53e3a51e0497ff6a6cbb367e760debc4194ae097498108df7b95eac2fa9bac4320077b510be3b7b823248bfe02ae501d9fe4ba179c7de6733c92bf89d523df9e31238ef497b9db719484cbab7531dbf6c5ea5a8087f95d59f5e4f89050e0f1dc03e464168ad76a64cca64b79",
            "b7203c6a6fb633d16e9cf5c1",
        )

        assertNull(MetaAttribution.app.testGetValue())
        assertNull(MetaAttribution.t.testGetValue())
        assertNull(MetaAttribution.data.testGetValue())
        assertNull(MetaAttribution.nonce.testGetValue())
        metaParams.recordMetaAttribution()

        val expectedApp = "12345"
        val expectedT = "1234567890"
        val expectedData = "afe56cf6228c6ea8c79da49186e718e92a579824596ae1d0d4d20d7793dca797bd4034ccf467bfae5c79a3981e7a2968c41949237e2b2db678c1c3d39c9ae564c5cafd52f2b77a3dc77bf1bae063114d0283b97417487207735da31ddc1531d5645a9c3e602c195a0ebf69c272aa5fda3a2d781cb47e117310164715a54c7a5a032740584e2789a7b4e596034c16425139a77e507c492b629c848573c714a03a2e7d25b9459b95842332b460f3682d19c35dbc7d53e3a51e0497ff6a6cbb367e760debc4194ae097498108df7b95eac2fa9bac4320077b510be3b7b823248bfe02ae501d9fe4ba179c7de6733c92bf89d523df9e31238ef497b9db719484cbab7531dbf6c5ea5a8087f95d59f5e4f89050e0f1dc03e464168ad76a64cca64b79"
        val expectedNonce = "b7203c6a6fb633d16e9cf5c1"

        val recordedApp = MetaAttribution.app.testGetValue()
        assertEquals(recordedApp, expectedApp)
        val recordedT = MetaAttribution.t.testGetValue()
        assertEquals(recordedT, expectedT)
        val recordedData = MetaAttribution.data.testGetValue()
        assertEquals(recordedData, expectedData)
        val recordedNonce = MetaAttribution.nonce.testGetValue()
        assertEquals(recordedNonce, expectedNonce)
    }
}
