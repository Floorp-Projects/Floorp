/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoWebExecutor
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class GeckoEngineTest {

    private val runtime: GeckoRuntime = mock(GeckoRuntime::class.java)
    private val context: Context = mock(Context::class.java)

    @Test
    fun createView() {
        assertTrue(GeckoEngine(context, runtime = runtime).createView(RuntimeEnvironment.application) is GeckoEngineView)
    }

    @Test
    fun createSession() {
        assertTrue(GeckoEngine(context, runtime = runtime).createSession() is GeckoEngineSession)
    }

    @Test
    fun name() {
        assertEquals("Gecko", GeckoEngine(context, runtime = runtime).name())
    }

    @Test
    fun settings() {
        val defaultSettings = DefaultSettings()
        val runtime = mock(GeckoRuntime::class.java)
        val runtimeSettings = mock(GeckoRuntimeSettings::class.java)
        `when`(runtimeSettings.javaScriptEnabled).thenReturn(true)
        `when`(runtimeSettings.webFontsEnabled).thenReturn(true)
        `when`(runtimeSettings.trackingProtectionCategories).thenReturn(TrackingProtectionPolicy.none().categories)
        `when`(runtime.settings).thenReturn(runtimeSettings)
        val engine = GeckoEngine(context, runtime = runtime, defaultSettings = defaultSettings)

        assertTrue(engine.settings.javascriptEnabled)
        engine.settings.javascriptEnabled = false
        verify(runtimeSettings).javaScriptEnabled = false

        assertTrue(engine.settings.webFontsEnabled)
        engine.settings.webFontsEnabled = false
        verify(runtimeSettings).webFontsEnabled = false

        assertFalse(engine.settings.remoteDebuggingEnabled)
        engine.settings.remoteDebuggingEnabled = true
        verify(runtimeSettings).remoteDebuggingEnabled = true

        assertFalse(engine.settings.testingModeEnabled)
        engine.settings.testingModeEnabled = true
        assertTrue(engine.settings.testingModeEnabled)

        assertNull(engine.settings.userAgentString)
        engine.settings.userAgentString = "test-ua"
        assertEquals("test-ua", engine.settings.userAgentString)

        assertEquals(TrackingProtectionPolicy.none(), engine.settings.trackingProtectionPolicy)
        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.all()
        verify(runtimeSettings).trackingProtectionCategories = TrackingProtectionPolicy.all().categories
        assertEquals(defaultSettings.trackingProtectionPolicy, TrackingProtectionPolicy.all())

        try {
            engine.settings.domStorageEnabled
            fail("Expected UnsupportedOperationException")
        } catch (e: UnsupportedSettingException) { }

        try {
            engine.settings.domStorageEnabled = false
            fail("Expected UnsupportedOperationException")
        } catch (e: UnsupportedSettingException) { }
    }

    @Test
    fun defaultSettings() {
        val runtime = mock(GeckoRuntime::class.java)
        val runtimeSettings = mock(GeckoRuntimeSettings::class.java)
        `when`(runtimeSettings.javaScriptEnabled).thenReturn(true)
        `when`(runtime.settings).thenReturn(runtimeSettings)

        val engine = GeckoEngine(context, DefaultSettings(
                trackingProtectionPolicy = TrackingProtectionPolicy.all(),
                javascriptEnabled = false,
                webFontsEnabled = false,
                remoteDebuggingEnabled = true,
                testingModeEnabled = true,
                userAgentString = "test-ua"), runtime)

        verify(runtimeSettings).javaScriptEnabled = false
        verify(runtimeSettings).webFontsEnabled = false
        verify(runtimeSettings).remoteDebuggingEnabled = true
        verify(runtimeSettings).trackingProtectionCategories = TrackingProtectionPolicy.all().categories
        assertTrue(engine.settings.testingModeEnabled)
        assertEquals("test-ua", engine.settings.userAgentString)
    }

    @Test
    fun `speculativeConnect forwards call to executor`() {
        val executor: GeckoWebExecutor = mock()

        val engine = GeckoEngine(context, runtime = runtime, executorProvider = { executor })

        engine.speculativeConnect("https://www.mozilla.org")

        verify(executor).speculativeConnect("https://www.mozilla.org")
    }
}