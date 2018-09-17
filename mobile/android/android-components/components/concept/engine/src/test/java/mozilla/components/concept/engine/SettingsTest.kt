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
        val settings = object : Settings() { }

        expectUnsupportedSettingException(
            { settings.javascriptEnabled },
            { settings.javascriptEnabled = false },
            { settings.domStorageEnabled },
            { settings.domStorageEnabled = false },
            { settings.webFontsEnabled },
            { settings.webFontsEnabled = false },
            { settings.trackingProtectionPolicy },
            { settings.trackingProtectionPolicy = TrackingProtectionPolicy.all() },
            { settings.requestInterceptor },
            { settings.requestInterceptor = null },
            { settings.userAgentString },
            { settings.userAgentString = null },
            { settings.mediaPlaybackRequiresUserGesture },
            { settings.mediaPlaybackRequiresUserGesture = false },
            { settings.javaScriptCanOpenWindowsAutomatically },
            { settings.javaScriptCanOpenWindowsAutomatically = false },
            { settings.displayZoomControls },
            { settings.displayZoomControls = false },
            { settings.loadWithOverviewMode },
            { settings.loadWithOverviewMode = false },
            { settings.allowFileAccess },
            { settings.allowFileAccess = false },
            { settings.allowContentAccess },
            { settings.allowContentAccess = false },
            { settings.allowFileAccessFromFileURLs },
            { settings.allowFileAccessFromFileURLs = false },
            { settings.allowUniversalAccessFromFileURLs },
            { settings.allowUniversalAccessFromFileURLs = false },
            { settings.verticalScrollBarEnabled },
            { settings.verticalScrollBarEnabled = false },
            { settings.horizontalScrollBarEnabled },
            { settings.horizontalScrollBarEnabled = false }
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
        assertNull(settings.userAgentString)
        assertTrue(settings.mediaPlaybackRequiresUserGesture)
        assertFalse(settings.javaScriptCanOpenWindowsAutomatically)
        assertTrue(settings.displayZoomControls)
        assertFalse(settings.loadWithOverviewMode)
        assertTrue(settings.allowContentAccess)
        assertTrue(settings.allowFileAccess)
        assertFalse(settings.allowFileAccessFromFileURLs)
        assertFalse(settings.allowUniversalAccessFromFileURLs)
        assertTrue(settings.verticalScrollBarEnabled)
        assertTrue(settings.horizontalScrollBarEnabled)

        val interceptor: RequestInterceptor = mock()

        val defaultSettings = DefaultSettings(
            javascriptEnabled = false,
            domStorageEnabled = false,
            webFontsEnabled = false,
            trackingProtectionPolicy = TrackingProtectionPolicy.all(),
            requestInterceptor = interceptor,
            userAgentString = "userAgent",
            mediaPlaybackRequiresUserGesture = false,
            javaScriptCanOpenWindowsAutomatically = true,
            displayZoomControls = false,
            loadWithOverviewMode = true,
            allowContentAccess = false,
            allowFileAccess = false,
            allowFileAccessFromFileURLs = true,
            allowUniversalAccessFromFileURLs = true,
            verticalScrollBarEnabled = false,
            horizontalScrollBarEnabled = false)

        assertFalse(defaultSettings.domStorageEnabled)
        assertFalse(defaultSettings.javascriptEnabled)
        assertFalse(defaultSettings.webFontsEnabled)
        assertEquals(TrackingProtectionPolicy.all(), defaultSettings.trackingProtectionPolicy)
        assertEquals(interceptor, defaultSettings.requestInterceptor)
        assertEquals("userAgent", defaultSettings.userAgentString)
        assertFalse(defaultSettings.mediaPlaybackRequiresUserGesture)
        assertTrue(defaultSettings.javaScriptCanOpenWindowsAutomatically)
        assertFalse(defaultSettings.displayZoomControls)
        assertTrue(defaultSettings.loadWithOverviewMode)
        assertFalse(defaultSettings.allowContentAccess)
        assertFalse(defaultSettings.allowFileAccess)
        assertTrue(defaultSettings.allowFileAccessFromFileURLs)
        assertTrue(defaultSettings.allowUniversalAccessFromFileURLs)
        assertFalse(defaultSettings.verticalScrollBarEnabled)
        assertFalse(defaultSettings.horizontalScrollBarEnabled)
    }
}