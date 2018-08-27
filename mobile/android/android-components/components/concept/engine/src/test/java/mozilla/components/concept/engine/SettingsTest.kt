/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import org.junit.Assert.fail
import org.junit.Test

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue

class SettingsTest {

    @Test
    fun testSettingsThrowByDefault() {
        val settings = object : Settings { }
        expectUnsupportedSettingException { settings.javascriptEnabled }
        expectUnsupportedSettingException { settings.javascriptEnabled = false }
        expectUnsupportedSettingException { settings.domStorageEnabled }
        expectUnsupportedSettingException { settings.domStorageEnabled = false }
        expectUnsupportedSettingException { settings.trackingProtectionPolicy }
        expectUnsupportedSettingException { settings.trackingProtectionPolicy = TrackingProtectionPolicy.all() }
    }

    private fun expectUnsupportedSettingException(f: () -> Unit) {
        try {
            f()
            fail("Expected UnsupportedSettingException")
        } catch (e: UnsupportedSettingException) { }
    }

    @Test
    fun testDefaultSettings() {
        val settings = DefaultSettings()
        assertTrue(settings.domStorageEnabled)
        assertTrue(settings.javascriptEnabled)
        assertNull(settings.trackingProtectionPolicy)

        val trackingProtectionSettings = DefaultSettings(false, false, TrackingProtectionPolicy.all())
        assertFalse(trackingProtectionSettings.domStorageEnabled)
        assertFalse(trackingProtectionSettings.javascriptEnabled)
        assertEquals(TrackingProtectionPolicy.all(), trackingProtectionSettings.trackingProtectionPolicy)
    }
}