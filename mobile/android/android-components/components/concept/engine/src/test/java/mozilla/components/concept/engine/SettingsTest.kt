/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.history.HistoryTrackingDelegate
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
    fun settingsThrowByDefault() {
        val settings = object : Settings() { }

        expectUnsupportedSettingException(
            { settings.javascriptEnabled },
            { settings.javascriptEnabled = false },
            { settings.domStorageEnabled },
            { settings.domStorageEnabled = false },
            { settings.webFontsEnabled },
            { settings.webFontsEnabled = false },
            { settings.automaticFontSizeAdjustment },
            { settings.automaticFontSizeAdjustment = false },
            { settings.trackingProtectionPolicy },
            { settings.trackingProtectionPolicy = TrackingProtectionPolicy.all() },
            { settings.historyTrackingDelegate },
            { settings.historyTrackingDelegate = null },
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
            { settings.horizontalScrollBarEnabled = false },
            { settings.remoteDebuggingEnabled },
            { settings.remoteDebuggingEnabled = false },
            { settings.supportMultipleWindows },
            { settings.supportMultipleWindows = false },
            { settings.testingModeEnabled },
            { settings.testingModeEnabled = false }
        )
    }

    private fun expectUnsupportedSettingException(vararg blocks: () -> Unit) {
        blocks.forEach { block ->
            expectException(UnsupportedSettingException::class, block)
        }
    }

    @Test
    fun defaultSettings() {
        val settings = DefaultSettings()
        assertTrue(settings.domStorageEnabled)
        assertTrue(settings.javascriptEnabled)
        assertNull(settings.trackingProtectionPolicy)
        assertNull(settings.historyTrackingDelegate)
        assertNull(settings.requestInterceptor)
        assertNull(settings.userAgentString)
        assertTrue(settings.mediaPlaybackRequiresUserGesture)
        assertFalse(settings.javaScriptCanOpenWindowsAutomatically)
        assertTrue(settings.displayZoomControls)
        assertTrue(settings.automaticFontSizeAdjustment)
        assertFalse(settings.loadWithOverviewMode)
        assertTrue(settings.allowContentAccess)
        assertTrue(settings.allowFileAccess)
        assertFalse(settings.allowFileAccessFromFileURLs)
        assertFalse(settings.allowUniversalAccessFromFileURLs)
        assertTrue(settings.verticalScrollBarEnabled)
        assertTrue(settings.horizontalScrollBarEnabled)
        assertFalse(settings.remoteDebuggingEnabled)
        assertFalse(settings.supportMultipleWindows)
        assertFalse(settings.testingModeEnabled)

        val interceptor: RequestInterceptor = mock()
        val historyTrackingDelegate: HistoryTrackingDelegate = mock()

        val defaultSettings = DefaultSettings(
            javascriptEnabled = false,
            domStorageEnabled = false,
            webFontsEnabled = false,
            automaticFontSizeAdjustment = false,
            trackingProtectionPolicy = TrackingProtectionPolicy.all(),
            historyTrackingDelegate = historyTrackingDelegate,
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
            horizontalScrollBarEnabled = false,
            remoteDebuggingEnabled = true,
            supportMultipleWindows = true,
            testingModeEnabled = true)

        assertFalse(defaultSettings.domStorageEnabled)
        assertFalse(defaultSettings.javascriptEnabled)
        assertFalse(defaultSettings.webFontsEnabled)
        assertFalse(defaultSettings.automaticFontSizeAdjustment)
        assertEquals(TrackingProtectionPolicy.all(), defaultSettings.trackingProtectionPolicy)
        assertEquals(historyTrackingDelegate, defaultSettings.historyTrackingDelegate)
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
        assertTrue(defaultSettings.remoteDebuggingEnabled)
        assertTrue(defaultSettings.supportMultipleWindows)
        assertTrue(defaultSettings.testingModeEnabled)
    }
}