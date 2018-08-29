/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.test.expectException
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class SettingsTest {

    @Test
    fun testSettingsThrowByDefault() {
        val settings = object : Settings { }

        expectUnsupportedSettingException(
            { settings.javascriptEnabled },
            { settings.javascriptEnabled = false },
            { settings.domStorageEnabled },
            { settings.domStorageEnabled = false },
            { settings.trackingProtectionPolicy },
            { settings.trackingProtectionPolicy = TrackingProtectionPolicy.all() },
            { settings.requestInterceptor },
            { settings.requestInterceptor = null }
        )
    }

    private fun expectUnsupportedSettingException(vararg blocks: () -> Unit) {
        blocks.forEach { block ->
            expectException(UnsupportedSettingException::class, block)
        }
    }

    @Test
    fun testDefaultSettings() {
        val settings = DefaultSettings()
        assertTrue(settings.domStorageEnabled)
        assertTrue(settings.javascriptEnabled)
        assertNull(settings.trackingProtectionPolicy)
        assertNull(settings.requestInterceptor)

        val interceptor: RequestInterceptor = mock()

        val trackingProtectionSettings = DefaultSettings(
            false,
            false,
            TrackingProtectionPolicy.all(),
            interceptor)

        assertFalse(trackingProtectionSettings.domStorageEnabled)
        assertFalse(trackingProtectionSettings.javascriptEnabled)
        assertEquals(TrackingProtectionPolicy.all(), trackingProtectionSettings.trackingProtectionPolicy)
        assertEquals(interceptor, trackingProtectionSettings.requestInterceptor)
    }
}