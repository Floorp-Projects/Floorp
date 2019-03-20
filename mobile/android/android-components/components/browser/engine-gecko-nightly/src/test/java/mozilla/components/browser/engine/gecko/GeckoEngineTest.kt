/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoWebExecutor
import org.robolectric.RobolectricTestRunner
import java.io.IOException
import org.mozilla.geckoview.WebExtension as GeckoWebExtension

@RunWith(RobolectricTestRunner::class)
class GeckoEngineTest {

    private val runtime: GeckoRuntime = mock(GeckoRuntime::class.java)
    private val context: Context = mock(Context::class.java)

    @Test
    @Ignore
    fun createView() {
        assertTrue(GeckoEngine(context, runtime = runtime).createView(
            ApplicationProvider.getApplicationContext()
        ) is GeckoEngineView)
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
        val contentBlockingSettings =
                ContentBlocking.Settings.Builder().categories(TrackingProtectionPolicy.none().categories).build()
        val runtime = mock(GeckoRuntime::class.java)
        val runtimeSettings = mock(GeckoRuntimeSettings::class.java)
        `when`(runtimeSettings.javaScriptEnabled).thenReturn(true)
        `when`(runtimeSettings.webFontsEnabled).thenReturn(true)
        `when`(runtimeSettings.contentBlocking).thenReturn(contentBlockingSettings)
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

        // Specifying no ua-string default should result in GeckoView's default.
        assertEquals(GeckoSession.getDefaultUserAgent(), engine.settings.userAgentString)
        // It also should be possible to read and set a new default.
        engine.settings.userAgentString = engine.settings.userAgentString + "-test"
        assertEquals(GeckoSession.getDefaultUserAgent() + "-test", engine.settings.userAgentString)

        assertEquals(TrackingProtectionPolicy.none(), engine.settings.trackingProtectionPolicy)
        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.all()
        assertEquals(TrackingProtectionPolicy.select(
                TrackingProtectionPolicy.AD,
                TrackingProtectionPolicy.SOCIAL,
                TrackingProtectionPolicy.ANALYTICS,
                TrackingProtectionPolicy.CONTENT,
                TrackingProtectionPolicy.TEST
        ).categories, contentBlockingSettings.categories)
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
        val contentBlockingSettings =
                ContentBlocking.Settings.Builder().categories(TrackingProtectionPolicy.none().categories).build()
        `when`(runtimeSettings.javaScriptEnabled).thenReturn(true)
        `when`(runtime.settings).thenReturn(runtimeSettings)
        `when`(runtimeSettings.contentBlocking).thenReturn(contentBlockingSettings)

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
        assertEquals(TrackingProtectionPolicy.select(
            TrackingProtectionPolicy.AD,
            TrackingProtectionPolicy.SOCIAL,
            TrackingProtectionPolicy.ANALYTICS,
            TrackingProtectionPolicy.CONTENT,
            TrackingProtectionPolicy.TEST
        ).categories, contentBlockingSettings.categories)
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

    @Test
    fun `install web extension successfully`() {
        val runtime = mock(GeckoRuntime::class.java)
        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        var result = GeckoResult<Void>()

        `when`(runtime.registerWebExtension(any())).thenReturn(result)
        engine.installWebExtension(
                WebExtension("test-webext", "resource://android/assets/extensions/test"),
                onSuccess = { onSuccessCalled = true },
                onError = { _, _ -> onErrorCalled = true }
        )
        result.complete(null)

        val extCaptor = argumentCaptor<GeckoWebExtension>()
        verify(runtime).registerWebExtension(extCaptor.capture())
        assertEquals("test-webext", extCaptor.value.id)
        assertEquals("resource://android/assets/extensions/test", extCaptor.value.location)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `install web extension failure`() {
        val runtime = mock(GeckoRuntime::class.java)
        val engine = GeckoEngine(context, runtime = runtime)
        var onErrorCalled = false
        val expected = IOException()
        var result = GeckoResult<Void>()

        var throwable: Throwable? = null
        `when`(runtime.registerWebExtension(any())).thenReturn(result)
        engine.installWebExtension(WebExtension("test-webext-error", "resource://android/assets/extensions/error")) { _, e ->
            onErrorCalled = true
            throwable = e
        }
        result.completeExceptionally(expected)

        assertTrue(onErrorCalled)
        assertEquals(expected, throwable)
    }

    @Test(expected = RuntimeException::class)
    fun `WHEN GeckoRuntime is shutting down THEN GeckoEngine throws runtime exception`() {
        val runtime: GeckoRuntime = mock()

        GeckoEngine(context, runtime = runtime)

        val captor = argumentCaptor<GeckoRuntime.Delegate>()
        verify(runtime).delegate = captor.capture()

        assertNotNull(captor.value)

        captor.value.onShutdown()
    }
}