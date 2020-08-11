/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.ext.getAntiTrackingPolicy
import mozilla.components.browser.engine.gecko.mediaquery.toGeckoValue
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.TrackingCategory
import mozilla.components.concept.engine.EngineSession.SafeBrowsingPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.ContentBlocking.CookieBehavior
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.ContentBlockingController.Event
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.MockWebExtension
import org.mozilla.geckoview.StorageController
import org.mozilla.geckoview.WebExtensionController
import org.mozilla.geckoview.WebPushController
import org.robolectric.Robolectric
import java.io.IOException
import java.lang.Exception
import org.mozilla.geckoview.WebExtension as GeckoWebExtension

@RunWith(AndroidJUnit4::class)
class GeckoEngineTest {

    private lateinit var runtime: GeckoRuntime
    private lateinit var context: Context

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
        context = mock()
    }

    @Test
    fun createView() {
        assertTrue(GeckoEngine(context, runtime = runtime).createView(
            Robolectric.buildActivity(Activity::class.java).get()
        ) is GeckoEngineView)
    }

    @Test
    fun createSession() {
        val engine = GeckoEngine(context, runtime = runtime)
        assertTrue(engine.createSession() is GeckoEngineSession)

        // Create a private speculative session and consume it
        engine.speculativeCreateSession(private = true)
        assertNotNull(engine.speculativeSession)
        var privateSpeculativeSession = engine.speculativeSession!!
        assertSame(privateSpeculativeSession, engine.createSession(private = true))
        assertNull(engine.speculativeSession)

        // Create a regular speculative session and make sure it is not returned
        // if a private session is requested instead.
        engine.speculativeCreateSession(private = false)
        assertNotNull(engine.speculativeSession)
        privateSpeculativeSession = engine.speculativeSession!!
        assertNotSame(privateSpeculativeSession, engine.createSession(private = true))
        // Make sure previous (never used) speculative session is now closed
        assertFalse(privateSpeculativeSession.geckoSession.isOpen)
        assertNull(engine.speculativeSession)
    }

    @Test
    fun speculativeCreateSession() {
        val engine = GeckoEngine(context, runtime = runtime)
        assertNull(engine.speculativeSession)

        // Create a private speculative session
        engine.speculativeCreateSession(private = true)
        assertNotNull(engine.speculativeSession)
        val privateSpeculativeSession = engine.speculativeSession!!
        assertTrue(privateSpeculativeSession.geckoSession.settings.usePrivateMode)

        // Creating another private speculative session should have no effect as
        // session hasn't been "consumed".
        engine.speculativeCreateSession(private = true)
        assertSame(privateSpeculativeSession, engine.speculativeSession)
        assertTrue(privateSpeculativeSession.geckoSession.settings.usePrivateMode)

        // Creating a non-private speculative session should affect prepared session
        engine.speculativeCreateSession(private = false)
        assertNotSame(privateSpeculativeSession, engine.speculativeSession)
        // Make sure previous (never used) speculative session is now closed
        assertFalse(privateSpeculativeSession.geckoSession.isOpen)
        val regularSpeculativeSession = engine.speculativeSession!!
        assertFalse(regularSpeculativeSession.geckoSession.settings.usePrivateMode)
    }

    @Test
    fun clearSpeculativeSession() {
        val engine = GeckoEngine(context, runtime = runtime)
        assertNull(engine.speculativeSession)

        val mockGeckoSession: GeckoSession = mock()
        val mockEngineSession: GeckoEngineSession = mock()
        whenever(mockEngineSession.geckoSession).thenReturn(mockGeckoSession)
        engine.speculativeSession = mockEngineSession
        engine.clearSpeculativeSession()
        verify(mockGeckoSession).close()
        assertNull(engine.speculativeSession)
    }

    @Test
    fun `createSession with contextId`() {
        val engine = GeckoEngine(context, runtime = runtime)

        // Create a speculative session with a context id and consume it
        engine.speculativeCreateSession(private = false, contextId = "1")
        assertNotNull(engine.speculativeSession)
        var newSpeculativeSession = engine.speculativeSession!!
        assertSame(newSpeculativeSession, engine.createSession(private = false, contextId = "1"))
        assertNull(engine.speculativeSession)

        // Create a regular speculative session and make sure it is not returned
        // if a session with a context id is requested instead.
        engine.speculativeCreateSession(private = false)
        assertNotNull(engine.speculativeSession)
        newSpeculativeSession = engine.speculativeSession!!
        assertNotSame(newSpeculativeSession, engine.createSession(private = false, contextId = "1"))
        // Make sure previous (never used) speculative session is now closed
        assertFalse(newSpeculativeSession.geckoSession.isOpen)
        assertNull(engine.speculativeSession)
    }

    @Test
    fun name() {
        assertEquals("Gecko", GeckoEngine(context, runtime = runtime).name())
    }

    @Test
    fun settings() {
        val defaultSettings = DefaultSettings()
        val contentBlockingSettings = ContentBlocking.Settings.Builder().build()
        val runtime = mock<GeckoRuntime>()
        val runtimeSettings = mock<GeckoRuntimeSettings>()
        whenever(runtimeSettings.javaScriptEnabled).thenReturn(true)
        whenever(runtimeSettings.webFontsEnabled).thenReturn(true)
        whenever(runtimeSettings.automaticFontSizeAdjustment).thenReturn(true)
        whenever(runtimeSettings.fontInflationEnabled).thenReturn(true)
        whenever(runtimeSettings.fontSizeFactor).thenReturn(1.0F)
        whenever(runtimeSettings.forceUserScalableEnabled).thenReturn(false)
        whenever(runtimeSettings.loginAutofillEnabled).thenReturn(false)
        whenever(runtimeSettings.contentBlocking).thenReturn(contentBlockingSettings)
        whenever(runtimeSettings.preferredColorScheme).thenReturn(GeckoRuntimeSettings.COLOR_SCHEME_SYSTEM)
        whenever(runtime.settings).thenReturn(runtimeSettings)
        val engine = GeckoEngine(context, runtime = runtime, defaultSettings = defaultSettings)

        assertTrue(engine.settings.javascriptEnabled)
        engine.settings.javascriptEnabled = false
        verify(runtimeSettings).javaScriptEnabled = false

        assertFalse(engine.settings.loginAutofillEnabled)
        engine.settings.loginAutofillEnabled = true
        verify(runtimeSettings).loginAutofillEnabled = true

        assertTrue(engine.settings.webFontsEnabled)
        engine.settings.webFontsEnabled = false
        verify(runtimeSettings).webFontsEnabled = false

        assertTrue(engine.settings.automaticFontSizeAdjustment)
        engine.settings.automaticFontSizeAdjustment = false
        verify(runtimeSettings).automaticFontSizeAdjustment = false

        assertTrue(engine.settings.fontInflationEnabled!!)
        engine.settings.fontInflationEnabled = null
        verify(runtimeSettings, never()).fontInflationEnabled = anyBoolean()
        engine.settings.fontInflationEnabled = false
        verify(runtimeSettings).fontInflationEnabled = false

        assertEquals(1.0F, engine.settings.fontSizeFactor)
        engine.settings.fontSizeFactor = null
        verify(runtimeSettings, never()).fontSizeFactor = anyFloat()
        engine.settings.fontSizeFactor = 2.0F
        verify(runtimeSettings).fontSizeFactor = 2.0F

        assertFalse(engine.settings.forceUserScalableContent)
        engine.settings.forceUserScalableContent = true
        verify(runtimeSettings).forceUserScalableEnabled = true

        assertFalse(engine.settings.remoteDebuggingEnabled)
        engine.settings.remoteDebuggingEnabled = true
        verify(runtimeSettings).remoteDebuggingEnabled = true

        assertFalse(engine.settings.testingModeEnabled)
        engine.settings.testingModeEnabled = true
        assertTrue(engine.settings.testingModeEnabled)

        assertEquals(PreferredColorScheme.System, engine.settings.preferredColorScheme)
        engine.settings.preferredColorScheme = PreferredColorScheme.Dark
        verify(runtimeSettings).preferredColorScheme = PreferredColorScheme.Dark.toGeckoValue()

        assertFalse(engine.settings.suspendMediaWhenInactive)
        engine.settings.suspendMediaWhenInactive = true
        assertEquals(true, engine.settings.suspendMediaWhenInactive)

        // Specifying no ua-string default should result in GeckoView's default.
        assertEquals(GeckoSession.getDefaultUserAgent(), engine.settings.userAgentString)
        // It also should be possible to read and set a new default.
        engine.settings.userAgentString = engine.settings.userAgentString + "-test"
        assertEquals(GeckoSession.getDefaultUserAgent() + "-test", engine.settings.userAgentString)

        assertEquals(null, engine.settings.trackingProtectionPolicy)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        val trackingStrictCategories = TrackingProtectionPolicy.strict().trackingCategories.sumBy { it.id }
        val artificialCategory =
            TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id
        assertEquals(
            trackingStrictCategories - artificialCategory,
            contentBlockingSettings.antiTrackingCategories
        )

        val safeStrictBrowsingCategories = SafeBrowsingPolicy.RECOMMENDED.id
        assertEquals(safeStrictBrowsingCategories, contentBlockingSettings.safeBrowsingCategories)

        engine.settings.safeBrowsingPolicy = arrayOf(SafeBrowsingPolicy.PHISHING)
        assertEquals(SafeBrowsingPolicy.PHISHING.id, contentBlockingSettings.safeBrowsingCategories)

        assertEquals(defaultSettings.trackingProtectionPolicy, TrackingProtectionPolicy.strict())
        assertEquals(contentBlockingSettings.cookieBehavior, CookiePolicy.ACCEPT_NON_TRACKERS.id)

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
    fun `the SCRIPTS_AND_SUB_RESOURCES tracking protection category must not be passed to gecko view`() {
        val mockRuntime = mock<GeckoRuntime>()
        val settings = spy(ContentBlocking.Settings.Builder().build())
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(settings)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        val trackingStrictCategories = TrackingProtectionPolicy.strict().trackingCategories.sumBy { it.id }
        val artificialCategory = TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id

        assertEquals(
            trackingStrictCategories - artificialCategory,
            mockRuntime.settings.contentBlocking.antiTrackingCategories
        )

        mockRuntime.settings.contentBlocking.setAntiTracking(0)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.select(
            arrayOf(TrackingCategory.SCRIPTS_AND_SUB_RESOURCES)
        )

        assertEquals(0, mockRuntime.settings.contentBlocking.antiTrackingCategories)
    }

    @Test
    fun `WHEN a strict tracking protection policy is set THEN the strict social list must be activated`() {
        val mockRuntime = mock<GeckoRuntime>()
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(mock())

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        verify(mockRuntime.settings.contentBlocking).setStrictSocialTrackingProtection(true)
    }

    @Test
    fun `WHEN a strict tracking protection policy is set THEN the setEnhancedTrackingProtectionLevel must be STRICT`() {
        val mockRuntime = mock<GeckoRuntime>()
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(mock())

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        verify(mockRuntime.settings.contentBlocking).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.STRICT
        )
    }

    @Test
    fun `setAntiTracking is only invoked when the value is changed`() {
        val mockRuntime = mock<GeckoRuntime>()
        val settings = spy(ContentBlocking.Settings.Builder().build())
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(settings)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)
        val policy = TrackingProtectionPolicy.recommended()

        engine.settings.trackingProtectionPolicy = policy

        verify(mockRuntime.settings.contentBlocking).setAntiTracking(
            policy.getAntiTrackingPolicy()
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = policy

        verify(mockRuntime.settings.contentBlocking, never()).setAntiTracking(
            policy.getAntiTrackingPolicy()
        )
    }

    @Test
    fun `cookiePurging is only invoked when the value is changed`() {
        val mockRuntime = mock<GeckoRuntime>()
        val settings = spy(ContentBlocking.Settings.Builder().build())
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(settings)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)
        val policy = TrackingProtectionPolicy.recommended()

        engine.settings.trackingProtectionPolicy = policy

        verify(mockRuntime.settings.contentBlocking).setCookiePurging(policy.cookiePurging)

        reset(settings)

        engine.settings.trackingProtectionPolicy = policy

        verify(mockRuntime.settings.contentBlocking, never()).setCookiePurging(policy.cookiePurging)
    }

    @Test
    fun `setCookieBehavior is only invoked when the value is changed`() {
        val mockRuntime = mock<GeckoRuntime>()
        val settings = spy(ContentBlocking.Settings.Builder().build())
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(settings)
        whenever(mockRuntime.settings.contentBlocking.cookieBehavior).thenReturn(CookieBehavior.ACCEPT_NONE)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)
        val policy = TrackingProtectionPolicy.recommended()

        engine.settings.trackingProtectionPolicy = policy

        verify(mockRuntime.settings.contentBlocking).setCookieBehavior(
            policy.cookiePolicy.id
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = policy

        verify(mockRuntime.settings.contentBlocking, never()).setCookieBehavior(
            policy.cookiePolicy.id
        )
    }

    @Test
    fun `setEnhancedTrackingProtectionLevel MUST always be set to STRICT unless the tracking protection policy is none`() {
        val mockRuntime = mock<GeckoRuntime>()
        val settings = spy(ContentBlocking.Settings.Builder().build())
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(settings)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.recommended()

        verify(mockRuntime.settings.contentBlocking).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.STRICT
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.recommended()

        verify(mockRuntime.settings.contentBlocking, never()).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.STRICT
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        verify(mockRuntime.settings.contentBlocking, never()).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.STRICT
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.none()
        verify(mockRuntime.settings.contentBlocking).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.NONE
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.none()
        verify(mockRuntime.settings.contentBlocking, never()).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.NONE
        )

        reset(settings)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        verify(mockRuntime.settings.contentBlocking).setEnhancedTrackingProtectionLevel(
            ContentBlocking.EtpLevel.STRICT
        )
    }

    @Test
    fun `WHEN a non strict tracking protection policy is set THEN the strict social list must be disabled`() {
        val mockRuntime = mock<GeckoRuntime>()
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking.strictSocialTrackingProtection).thenReturn(true)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.recommended()

        verify(mockRuntime.settings.contentBlocking).setStrictSocialTrackingProtection(false)
    }

    @Test
    fun `WHEN strict social tracking protection is set to true THEN the strict social list must be activated`() {
        val mockRuntime = mock<GeckoRuntime>()
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(mock())

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.select(
            strictSocialTrackingProtection = true
        )

        verify(mockRuntime.settings.contentBlocking).setStrictSocialTrackingProtection(true)
    }

    @Test
    fun `WHEN strict social tracking protection is set to false THEN the strict social list must be disabled`() {
        val mockRuntime = mock<GeckoRuntime>()
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking.strictSocialTrackingProtection).thenReturn(true)

        val engine = GeckoEngine(testContext, runtime = mockRuntime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.select(
            strictSocialTrackingProtection = false
        )

        verify(mockRuntime.settings.contentBlocking).setStrictSocialTrackingProtection(false)
    }

    @Test
    fun defaultSettings() {
        val runtime = mock<GeckoRuntime>()
        val runtimeSettings = mock<GeckoRuntimeSettings>()
        val contentBlockingSettings = ContentBlocking.Settings.Builder().build()
        whenever(runtimeSettings.javaScriptEnabled).thenReturn(true)
        whenever(runtime.settings).thenReturn(runtimeSettings)
        whenever(runtimeSettings.contentBlocking).thenReturn(contentBlockingSettings)
        whenever(runtimeSettings.fontInflationEnabled).thenReturn(true)

        val engine = GeckoEngine(context,
            DefaultSettings(
                trackingProtectionPolicy = TrackingProtectionPolicy.strict(),
                javascriptEnabled = false,
                webFontsEnabled = false,
                automaticFontSizeAdjustment = false,
                fontInflationEnabled = false,
                fontSizeFactor = 2.0F,
                remoteDebuggingEnabled = true,
                testingModeEnabled = true,
                userAgentString = "test-ua",
                preferredColorScheme = PreferredColorScheme.Light,
                suspendMediaWhenInactive = true,
                forceUserScalableContent = false
            ), runtime)

        verify(runtimeSettings).javaScriptEnabled = false
        verify(runtimeSettings).webFontsEnabled = false
        verify(runtimeSettings).automaticFontSizeAdjustment = false
        verify(runtimeSettings).fontInflationEnabled = false
        verify(runtimeSettings).fontSizeFactor = 2.0F
        verify(runtimeSettings).remoteDebuggingEnabled = true
        verify(runtimeSettings).forceUserScalableEnabled = false

        val trackingStrictCategories = TrackingProtectionPolicy.strict().trackingCategories.sumBy { it.id }
        val artificialCategory =
            TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id
        assertEquals(
            trackingStrictCategories - artificialCategory,
            contentBlockingSettings.antiTrackingCategories
        )

        assertEquals(SafeBrowsingPolicy.RECOMMENDED.id, contentBlockingSettings.safeBrowsingCategories)

        assertEquals(CookiePolicy.ACCEPT_NON_TRACKERS.id, contentBlockingSettings.cookieBehavior)
        assertTrue(engine.settings.testingModeEnabled)
        assertEquals("test-ua", engine.settings.userAgentString)
        assertEquals(PreferredColorScheme.Light, engine.settings.preferredColorScheme)
        assertTrue(engine.settings.suspendMediaWhenInactive)

        engine.settings.safeBrowsingPolicy = arrayOf(SafeBrowsingPolicy.PHISHING)
        engine.settings.trackingProtectionPolicy =
            TrackingProtectionPolicy.select(
                trackingCategories = arrayOf(TrackingProtectionPolicy.TrackingCategory.AD),
                cookiePolicy = CookiePolicy.ACCEPT_ONLY_FIRST_PARTY
            )

        assertEquals(
            TrackingProtectionPolicy.TrackingCategory.AD.id,
            contentBlockingSettings.antiTrackingCategories
        )

        assertEquals(
            SafeBrowsingPolicy.PHISHING.id,
            contentBlockingSettings.safeBrowsingCategories
        )

        assertEquals(
            CookiePolicy.ACCEPT_ONLY_FIRST_PARTY.id,
            contentBlockingSettings.cookieBehavior
        )

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.none()

        assertEquals(CookiePolicy.ACCEPT_ALL.id, contentBlockingSettings.cookieBehavior)
    }

    @Test
    fun `speculativeConnect forwards call to executor`() {
        val executor: GeckoWebExecutor = mock()

        val engine = GeckoEngine(context, runtime = runtime, executorProvider = { executor })

        engine.speculativeConnect("https://www.mozilla.org")

        verify(executor).speculativeConnect("https://www.mozilla.org")
    }

    @Test
    fun `install built-in web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "resource://android/assets/extensions/test"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        val result = GeckoResult<GeckoWebExtension>()

        whenever(extensionController.ensureBuiltIn(extUrl, extId)).thenReturn(result)
        engine.installWebExtension(
            extId,
            extUrl,
            onSuccess = { onSuccessCalled = true },
            onError = { _, _ -> onErrorCalled = true }
        )
        result.complete(mockNativeExtension(extId, extUrl))

        val extUrlCaptor = argumentCaptor<String>()
        val extIdCaptor = argumentCaptor<String>()
        verify(extensionController).ensureBuiltIn(extUrlCaptor.capture(), extIdCaptor.capture())
        assertEquals(extUrl, extUrlCaptor.value)
        assertEquals(extId, extIdCaptor.value)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `install external web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "https://addons.mozilla.org/firefox/downloads/file/123/some_web_ext.xpi"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        val result = GeckoResult<GeckoWebExtension>()

        whenever(extensionController.install(any())).thenReturn(result)
        engine.installWebExtension(
            extId,
            extUrl,
            onSuccess = { onSuccessCalled = true },
            onError = { _, _ -> onErrorCalled = true }
        )
        result.complete(mockNativeExtension(extId, extUrl))

        val extCaptor = argumentCaptor<String>()
        verify(extensionController).install(extCaptor.capture())
        assertEquals(extUrl, extCaptor.value)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `install built-in web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "resource://android/assets/extensions/test"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        var onErrorCalled = false
        val expected = IOException()
        val result = GeckoResult<GeckoWebExtension>()

        var throwable: Throwable? = null
        whenever(extensionController.ensureBuiltIn(extUrl, extId)).thenReturn(result)
        engine.installWebExtension(extId, extUrl) { _, e ->
            onErrorCalled = true
            throwable = e
        }
        result.completeExceptionally(expected)

        assertTrue(onErrorCalled)
        assertEquals(expected, throwable)
    }

    @Test
    fun `install external web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "https://addons.mozilla.org/firefox/downloads/file/123/some_web_ext.xpi"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        var onErrorCalled = false
        val expected = IOException()
        val result = GeckoResult<GeckoWebExtension>()

        var throwable: Throwable? = null
        whenever(extensionController.install(any())).thenReturn(result)
        engine.installWebExtension(extId, extUrl) { _, e ->
            onErrorCalled = true
            throwable = e
        }
        result.completeExceptionally(expected)

        assertTrue(onErrorCalled)
        assertEquals(expected, throwable)
    }

    @Test
    fun `uninstall web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val nativeExtension = mockNativeExtension("test-webext", "https://addons.mozilla.org/1/some_web_ext.xpi")
        val ext = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            nativeExtension,
            runtime
        )

        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        var onSuccessCalled = false
        var onErrorCalled = false
        val result = GeckoResult<Void>()

        whenever(extensionController.uninstall(any())).thenReturn(result)
        engine.uninstallWebExtension(
            ext,
            onSuccess = { onSuccessCalled = true },
            onError = { _, _ -> onErrorCalled = true }
        )
        result.complete(null)
        verify(webExtensionsDelegate).onUninstalled(ext)

        val extCaptor = argumentCaptor<GeckoWebExtension>()
        verify(extensionController).uninstall(extCaptor.capture())
        assertSame(nativeExtension, extCaptor.value)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `uninstall web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val nativeExtension = mockNativeExtension(
            "test-webext",
            "https://addons.mozilla.org/firefox/downloads/file/123/some_web_ext.xpi"
        )
        val ext = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            nativeExtension,
            runtime
        )

        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        var onErrorCalled = false
        val expected = IOException()
        val result = GeckoResult<Void>()

        var throwable: Throwable? = null
        whenever(extensionController.uninstall(any())).thenReturn(result)
        engine.uninstallWebExtension(ext) { _, e ->
            onErrorCalled = true
            throwable = e
        }
        result.completeExceptionally(expected)
        verify(webExtensionsDelegate, never()).onUninstalled(ext)

        assertTrue(onErrorCalled)
        assertEquals(expected, throwable)
    }

    @Test
    fun `web extension delegate handles installation of built-in extensions`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extId = "test-webext"
        val extUrl = "resource://android/assets/extensions/test"
        val result = GeckoResult<GeckoWebExtension>()
        whenever(webExtensionController.ensureBuiltIn(extUrl, extId)).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        result.complete(mockNativeExtension(extId, extUrl))

        val extCaptor = argumentCaptor<WebExtension>()
        verify(webExtensionsDelegate).onInstalled(extCaptor.capture())
        assertEquals(extId, extCaptor.value.id)
        assertEquals(extUrl, extCaptor.value.url)
    }

    @Test
    fun `web extension delegate handles installation of external extensions`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extId = "test-webext"
        val extUrl = "https://addons.mozilla.org/firefox/downloads/123/some_web_ext.xpi"
        val result = GeckoResult<GeckoWebExtension>()
        whenever(webExtensionController.install(any())).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        result.complete(mockNativeExtension(extId, extUrl))

        val extCaptor = argumentCaptor<WebExtension>()
        verify(webExtensionsDelegate).onInstalled(extCaptor.capture())
        assertEquals(extId, extCaptor.value.id)
        assertEquals(extUrl, extCaptor.value.url)
    }

    @Test
    fun `web extension delegate handles install prompt`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val extension = mockNativeExtension("test", "uri")
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val geckoDelegateCaptor = argumentCaptor<WebExtensionController.PromptDelegate>()
        verify(webExtensionController).promptDelegate = geckoDelegateCaptor.capture()

        assertEquals(GeckoResult.DENY, geckoDelegateCaptor.value.onInstallPrompt(extension))
        val extensionCaptor = argumentCaptor<WebExtension>()
        verify(webExtensionsDelegate).onInstallPermissionRequest(extensionCaptor.capture())
        val capturedExtension =
                extensionCaptor.value as mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension
        assertEquals(extension, capturedExtension.nativeExtension)

        whenever(webExtensionsDelegate.onInstallPermissionRequest(any())).thenReturn(true)
        assertEquals(GeckoResult.ALLOW, geckoDelegateCaptor.value.onInstallPrompt(extension))
    }

    @Test
    fun `web extension delegate handles update prompt`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val currentExtension = mockNativeExtension("test", "uri")
        val updatedExtension = mockNativeExtension("testUpdated", "uri")
        val updatedPermissions = arrayOf("p1", "p2")
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val geckoDelegateCaptor = argumentCaptor<WebExtensionController.PromptDelegate>()
        verify(webExtensionController).promptDelegate = geckoDelegateCaptor.capture()

        val result = geckoDelegateCaptor.value.onUpdatePrompt(
            currentExtension, updatedExtension, updatedPermissions, emptyArray()
        )
        assertNotNull(result)

        val currentExtensionCaptor = argumentCaptor<WebExtension>()
        val updatedExtensionCaptor = argumentCaptor<WebExtension>()
        val onPermissionsGrantedCaptor = argumentCaptor<((Boolean) -> Unit)>()
        verify(webExtensionsDelegate).onUpdatePermissionRequest(
            currentExtensionCaptor.capture(),
            updatedExtensionCaptor.capture(),
            eq(updatedPermissions.toList()),
            onPermissionsGrantedCaptor.capture()
        )
        val current =
            currentExtensionCaptor.value as mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension
        assertEquals(currentExtension, current.nativeExtension)
        val updated =
            updatedExtensionCaptor.value as mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension
        assertEquals(updatedExtension, updated.nativeExtension)

        onPermissionsGrantedCaptor.value.invoke(true)
        assertEquals(GeckoResult.ALLOW, result)
    }

    @Test
    fun `web extension delegate notified of browser actions from built-in extensions`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "resource://android/assets/extensions/test"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val result = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.ensureBuiltIn(extUrl, extId)).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        val extension = mockNativeExtension(extId, extUrl)
        result.complete(extension)

        val actionDelegateCaptor = argumentCaptor<org.mozilla.geckoview.WebExtension.ActionDelegate>()
        verify(extension).setActionDelegate(actionDelegateCaptor.capture())

        val browserAction: org.mozilla.geckoview.WebExtension.Action = mock()
        actionDelegateCaptor.value.onBrowserAction(extension, null, browserAction)

        val extensionCaptor = argumentCaptor<WebExtension>()
        val actionCaptor = argumentCaptor<Action>()
        verify(webExtensionsDelegate).onBrowserActionDefined(extensionCaptor.capture(), actionCaptor.capture())
        assertEquals(extId, extensionCaptor.value.id)

        actionCaptor.value.onClick()
        verify(browserAction).click()
    }

    @Test
    fun `web extension delegate notified of page actions from built-in extensions`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "resource://android/assets/extensions/test"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val result = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.ensureBuiltIn(extUrl, extId)).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        val extension = mockNativeExtension(extId, extUrl)
        result.complete(extension)

        val actionDelegateCaptor = argumentCaptor<org.mozilla.geckoview.WebExtension.ActionDelegate>()
        verify(extension).setActionDelegate(actionDelegateCaptor.capture())

        val pageAction: org.mozilla.geckoview.WebExtension.Action = mock()
        actionDelegateCaptor.value.onPageAction(extension, null, pageAction)

        val extensionCaptor = argumentCaptor<WebExtension>()
        val actionCaptor = argumentCaptor<Action>()
        verify(webExtensionsDelegate).onPageActionDefined(extensionCaptor.capture(), actionCaptor.capture())
        assertEquals(extId, extensionCaptor.value.id)

        actionCaptor.value.onClick()
        verify(pageAction).click()
    }

    @Test
    fun `web extension delegate notified when built-in extension wants to open tab`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "resource://android/assets/extensions/test"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val result = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.ensureBuiltIn(extUrl, extId)).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        val extension = mockNativeExtension(extId, extUrl)
        result.complete(extension)

        val tabDelegateCaptor = argumentCaptor<org.mozilla.geckoview.WebExtension.TabDelegate>()
        verify(extension).tabDelegate = tabDelegateCaptor.capture()

        val createTabDetails: org.mozilla.geckoview.WebExtension.CreateTabDetails = mock()
        tabDelegateCaptor.value.onNewTab(extension, createTabDetails)

        val extensionCaptor = argumentCaptor<WebExtension>()
        verify(webExtensionsDelegate).onNewTab(extensionCaptor.capture(), any(), eq(false), eq(""))
        assertEquals(extId, extensionCaptor.value.id)
    }

    @Test
    fun `web extension delegate notified of browser actions from external extensions`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "https://addons.mozilla.org/firefox/downloads/file/123/some_web_ext.xpi"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val result = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.install(any())).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        val extension = mockNativeExtension(extId, extUrl)
        result.complete(extension)

        val actionDelegateCaptor = argumentCaptor<org.mozilla.geckoview.WebExtension.ActionDelegate>()
        verify(extension).setActionDelegate(actionDelegateCaptor.capture())

        val browserAction: org.mozilla.geckoview.WebExtension.Action = mock()
        actionDelegateCaptor.value.onBrowserAction(extension, null, browserAction)

        val extensionCaptor = argumentCaptor<WebExtension>()
        val actionCaptor = argumentCaptor<Action>()
        verify(webExtensionsDelegate).onBrowserActionDefined(extensionCaptor.capture(), actionCaptor.capture())
        assertEquals(extId, extensionCaptor.value.id)

        actionCaptor.value.onClick()
        verify(browserAction).click()
    }

    @Test
    fun `web extension delegate notified of page actions from external extensions`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "https://addons.mozilla.org/firefox/downloads/file/123/some_web_ext.xpi"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val result = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.install(any())).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        val extension = mockNativeExtension(extId, extUrl)
        result.complete(extension)

        val actionDelegateCaptor = argumentCaptor<org.mozilla.geckoview.WebExtension.ActionDelegate>()
        verify(extension).setActionDelegate(actionDelegateCaptor.capture())

        val pageAction: org.mozilla.geckoview.WebExtension.Action = mock()
        actionDelegateCaptor.value.onPageAction(extension, null, pageAction)

        val extensionCaptor = argumentCaptor<WebExtension>()
        val actionCaptor = argumentCaptor<Action>()
        verify(webExtensionsDelegate).onPageActionDefined(extensionCaptor.capture(), actionCaptor.capture())
        assertEquals(extId, extensionCaptor.value.id)

        actionCaptor.value.onClick()
        verify(pageAction).click()
    }

    @Test
    fun `web extension delegate notified when external extension wants to open tab`() {
        val runtime = mock<GeckoRuntime>()
        val extId = "test-webext"
        val extUrl = "https://addons.mozilla.org/firefox/downloads/file/123/some_web_ext.xpi"

        val extensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val result = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.install(any())).thenReturn(result)
        engine.installWebExtension(extId, extUrl)
        val extension = mockNativeExtension(extId, extUrl)
        result.complete(extension)

        val tabDelegateCaptor = argumentCaptor<org.mozilla.geckoview.WebExtension.TabDelegate>()
        verify(extension).tabDelegate = tabDelegateCaptor.capture()

        val createTabDetails: org.mozilla.geckoview.WebExtension.CreateTabDetails = mock()
        tabDelegateCaptor.value.onNewTab(extension, createTabDetails)

        val extensionCaptor = argumentCaptor<WebExtension>()
        verify(webExtensionsDelegate).onNewTab(extensionCaptor.capture(), any(), eq(false), eq(""))
        assertEquals(extId, extensionCaptor.value.id)
    }

    @Test
    fun `web extension delegate notified of extension list change`() {
        val runtime: GeckoRuntime = mock()
        val webExtensionController: WebExtensionController = mock()
        whenever(runtime.webExtensionController).thenReturn(webExtensionController)

        val webExtensionsDelegate: WebExtensionDelegate = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val debuggerDelegateCaptor = argumentCaptor<WebExtensionController.DebuggerDelegate>()
        verify(webExtensionController).setDebuggerDelegate(debuggerDelegateCaptor.capture())

        debuggerDelegateCaptor.value.onExtensionListUpdated()
        verify(webExtensionsDelegate).onExtensionListUpdated()
    }

    @Test
    fun `update web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val updatedExtension = MockWebExtension(bundle)
        val updateExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.update(any())).thenReturn(updateExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        var onErrorCalled = false

        engine.updateWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { _, _ -> onErrorCalled = true }
        )
        updateExtensionResult.complete(updatedExtension)

        assertFalse(onErrorCalled)
        assertNotNull(result)
    }

    @Test
    fun `try to update a web extension without a new update available`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val updateExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.update(any())).thenReturn(updateExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        var onErrorCalled = false

        engine.updateWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { _, _ -> onErrorCalled = true }
        )
        updateExtensionResult.complete(null)

        assertFalse(onErrorCalled)
        assertNull(result)
    }

    @Test
    fun `update web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val updateExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.update(any())).thenReturn(updateExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        val expected = IOException()
        var throwable: Throwable? = null

        engine.updateWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { _, e -> throwable = e }
        )
        updateExtensionResult.completeExceptionally(expected)

        assertSame(expected, throwable)
        assertNull(result)
    }

    @Test
    fun `list web extensions successfully`() {
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val installedExtension = MockWebExtension(bundle)

        val installedExtensions = listOf<GeckoWebExtension>(installedExtension)
        val installedExtensionResult = GeckoResult<List<GeckoWebExtension>>()

        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()
        whenever(extensionController.list()).thenReturn(installedExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(testContext, runtime = runtime)
        var extensions: List<WebExtension>? = null
        var onErrorCalled = false

        engine.listInstalledWebExtensions(
            onSuccess = { extensions = it },
            onError = { onErrorCalled = true }
        )
        installedExtensionResult.complete(installedExtensions)

        assertFalse(onErrorCalled)
        assertNotNull(extensions)
    }

    @Test
    fun `migrate private browsing default`() {
        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val metaDataBundle = GeckoBundle()
        metaDataBundle.putBoolean("privateBrowsingAllowed", true)
        metaDataBundle.putStringArray("disabledFlags", emptyArray())
        bundle.putBundle("metaData", metaDataBundle)
        val installedExtension = MockWebExtension(bundle)

        val installedExtensions = listOf<GeckoWebExtension>(installedExtension)
        val installedExtensionResult = GeckoResult<List<GeckoWebExtension>>()

        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()
        whenever(extensionController.list()).thenReturn(installedExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val allowedInPrivateBrowsingExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.setAllowedInPrivateBrowsing(any(), anyBoolean())).thenReturn(allowedInPrivateBrowsingExtensionResult)

        val engine = spy(GeckoEngine(testContext, runtime = runtime))
        var extensions: List<WebExtension>? = null
        var onErrorCalled = false

        engine.listInstalledWebExtensions(
                onSuccess = { extensions = it },
                onError = { onErrorCalled = true }
        )
        installedExtensionResult.complete(installedExtensions)
        assertFalse(onErrorCalled)
        assertNotNull(extensions)
        val installedWebExtension = extensions!![0]
        verify(engine, times(1)).setAllowedInPrivateBrowsing(eq(installedWebExtension), eq(false), any(), any())

        engine.listInstalledWebExtensions(
                onSuccess = { extensions = it },
                onError = { onErrorCalled = true }
        )
        verify(engine, times(1)).setAllowedInPrivateBrowsing(eq(installedWebExtension), eq(false), any(), any())
    }

    @Test
    fun `list web extensions failure`() {
        val installedExtensionResult = GeckoResult<List<GeckoWebExtension>>()

        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()
        whenever(extensionController.list()).thenReturn(installedExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        var extensions: List<WebExtension>? = null
        val expected = IOException()
        var throwable: Throwable? = null

        engine.listInstalledWebExtensions(
            onSuccess = { extensions = it },
            onError = { throwable = it }
        )
        installedExtensionResult.completeExceptionally(expected)

        assertSame(expected, throwable)
        assertNull(extensions)
    }

    @Test
    fun `enable web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val enabledExtension = MockWebExtension(bundle)
        val enableExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.enable(any(), anyInt())).thenReturn(enableExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        var result: WebExtension? = null
        var onErrorCalled = false

        engine.enableWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { onErrorCalled = true }
        )
        enableExtensionResult.complete(enabledExtension)

        assertFalse(onErrorCalled)
        assertNotNull(result)
        verify(webExtensionsDelegate).onEnabled(result!!)
    }

    @Test
    fun `enable web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val enableExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.enable(any(), anyInt())).thenReturn(enableExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        val expected = IOException()
        var throwable: Throwable? = null

        engine.enableWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { throwable = it }
        )
        enableExtensionResult.completeExceptionally(expected)

        assertSame(expected, throwable)
        assertNull(result)
        verify(webExtensionsDelegate, never()).onEnabled(any())
    }

    @Test
    fun `disable web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val disabledExtension = MockWebExtension(bundle)
        val disableExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.disable(any(), anyInt())).thenReturn(disableExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        var onErrorCalled = false

        engine.disableWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { onErrorCalled = true }
        )
        disableExtensionResult.complete(disabledExtension)

        assertFalse(onErrorCalled)
        assertNotNull(result)
        verify(webExtensionsDelegate).onDisabled(result!!)
    }

    @Test
    fun `disable web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val disableExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.disable(any(), anyInt())).thenReturn(disableExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        val expected = IOException()
        var throwable: Throwable? = null

        engine.disableWebExtension(
            extension,
            onSuccess = { result = it },
            onError = { throwable = it }
        )
        disableExtensionResult.completeExceptionally(expected)

        assertSame(expected, throwable)
        assertNull(result)
        verify(webExtensionsDelegate, never()).onEnabled(any())
    }

    @Test
    fun `set allowedInPrivateBrowsing successfully`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val allowedInPrivateBrowsing = MockWebExtension(bundle)
        val allowedInPrivateBrowsingExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.setAllowedInPrivateBrowsing(any(), anyBoolean())).thenReturn(allowedInPrivateBrowsingExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        var onErrorCalled = false

        engine.setAllowedInPrivateBrowsing(
            extension,
            true,
            onSuccess = { ext -> result = ext },
            onError = { onErrorCalled = true }
        )
        allowedInPrivateBrowsingExtensionResult.complete(allowedInPrivateBrowsing)

        assertFalse(onErrorCalled)
        assertNotNull(result)
        verify(webExtensionsDelegate).onAllowedInPrivateBrowsingChanged(result!!)
    }

    @Test
    fun `set allowedInPrivateBrowsing failure`() {
        val runtime = mock<GeckoRuntime>()
        val extensionController: WebExtensionController = mock()

        val bundle = GeckoBundle()
        bundle.putString("webExtensionId", "id")
        bundle.putString("locationURI", "uri")
        val allowedInPrivateBrowsingExtensionResult = GeckoResult<GeckoWebExtension>()
        whenever(extensionController.setAllowedInPrivateBrowsing(any(), anyBoolean())).thenReturn(allowedInPrivateBrowsingExtensionResult)
        whenever(runtime.webExtensionController).thenReturn(extensionController)

        val engine = GeckoEngine(context, runtime = runtime)
        val webExtensionsDelegate: WebExtensionDelegate = mock()
        engine.registerWebExtensionDelegate(webExtensionsDelegate)

        val extension = mozilla.components.browser.engine.gecko.webextension.GeckoWebExtension(
            mockNativeExtension(),
            runtime
        )
        var result: WebExtension? = null
        val expected = IOException()
        var throwable: Throwable? = null

        engine.setAllowedInPrivateBrowsing(
            extension,
            true,
            onSuccess = { ext -> result = ext },
            onError = { throwable = it }
        )
        allowedInPrivateBrowsingExtensionResult.completeExceptionally(expected)

        assertSame(expected, throwable)
        assertNull(result)
        verify(webExtensionsDelegate, never()).onAllowedInPrivateBrowsingChanged(any())
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

    @Test
    fun `clear browsing data for all hosts`() {
        val runtime: GeckoRuntime = mock()
        val storageController: StorageController = mock()

        var onSuccessCalled = false

        val result = GeckoResult<Void>()
        whenever(runtime.storageController).thenReturn(storageController)
        whenever(storageController.clearData(eq(Engine.BrowsingData.all().types.toLong()))).thenReturn(result)
        result.complete(null)

        val engine = GeckoEngine(context, runtime = runtime)
        engine.clearData(data = Engine.BrowsingData.all(), onSuccess = { onSuccessCalled = true })
        assertTrue(onSuccessCalled)
    }

    @Test
    fun `error handler invoked when clearing browsing data for all hosts fails`() {
        val runtime: GeckoRuntime = mock()
        val storageController: StorageController = mock()

        var throwable: Throwable? = null
        var onErrorCalled = false

        val exception = IOException()
        val result = GeckoResult<Void>()
        whenever(runtime.storageController).thenReturn(storageController)
        whenever(storageController.clearData(eq(Engine.BrowsingData.all().types.toLong()))).thenReturn(result)
        result.completeExceptionally(exception)

        val engine = GeckoEngine(context, runtime = runtime)
        engine.clearData(data = Engine.BrowsingData.all(), onError = {
            onErrorCalled = true
            throwable = it
        })
        assertTrue(onErrorCalled)
        assertSame(exception, throwable)
    }

    @Test
    fun `clear browsing data for specified host`() {
        val runtime: GeckoRuntime = mock()
        val storageController: StorageController = mock()

        var onSuccessCalled = false

        val result = GeckoResult<Void>()
        whenever(runtime.storageController).thenReturn(storageController)
        whenever(storageController.clearDataFromHost(
                eq("mozilla.org"),
                eq(Engine.BrowsingData.all().types.toLong()))
        ).thenReturn(result)
        result.complete(null)

        val engine = GeckoEngine(context, runtime = runtime)
        engine.clearData(data = Engine.BrowsingData.all(), host = "mozilla.org", onSuccess = { onSuccessCalled = true })
        assertTrue(onSuccessCalled)
    }

    @Test
    fun `error handler invoked when clearing browsing data for specified hosts fails`() {
        val runtime: GeckoRuntime = mock()
        val storageController: StorageController = mock()

        var throwable: Throwable? = null
        var onErrorCalled = false

        val exception = IOException()
        val result = GeckoResult<Void>()
        whenever(runtime.storageController).thenReturn(storageController)
        whenever(storageController.clearDataFromHost(
                eq("mozilla.org"),
                eq(Engine.BrowsingData.all().types.toLong()))
        ).thenReturn(result)
        result.completeExceptionally(exception)

        val engine = GeckoEngine(context, runtime = runtime)
        engine.clearData(data = Engine.BrowsingData.all(), host = "mozilla.org", onError = {
            onErrorCalled = true
            throwable = it
        })
        assertTrue(onErrorCalled)
        assertSame(exception, throwable)
    }

    @Test
    fun `test parsing engine version`() {
        val runtime: GeckoRuntime = mock()
        val engine = GeckoEngine(context, runtime = runtime)
        val version = engine.version

        println(version)

        assertTrue(version.major >= 69)
        assertTrue(version.isAtLeast(69, 0, 0))
    }

    @Test
    fun `after init is called the trackingProtectionExceptionStore must be restored`() {
        val mockStore: TrackingProtectionExceptionStorage = mock()
        val runtime: GeckoRuntime = mock()
        GeckoEngine(context, runtime = runtime, trackingProtectionExceptionStore = mockStore)

        verify(mockStore).restore()
    }

    @Test
    fun `fetch trackers logged successfully`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        val mockSession = mock<GeckoEngineSession>()
        val mockGeckoSetting = mock<GeckoRuntimeSettings>()
        val mockGeckoContentBlockingSetting = mock<ContentBlocking.Settings>()
        var trackersLog: List<TrackerLog>? = null

        val mockContentBlockingController = mock<ContentBlockingController>()
        var logEntriesResult = GeckoResult<List<ContentBlockingController.LogEntry>>()

        whenever(runtime.settings).thenReturn(mockGeckoSetting)
        whenever(mockGeckoSetting.contentBlocking).thenReturn(mockGeckoContentBlockingSetting)
        whenever(mockGeckoContentBlockingSetting.getEnhancedTrackingProtectionLevel()).thenReturn(
            ContentBlocking.EtpLevel.STRICT
        )
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlockingController)
        whenever(mockContentBlockingController.getLog(any())).thenReturn(logEntriesResult)

        engine.getTrackersLog(
            mockSession,
            onSuccess = {
                trackersLog = it
                onSuccessCalled = true
            },
            onError = { onErrorCalled = true }
        )

        logEntriesResult.complete(createDummyLogEntryList())

        val trackerLog = trackersLog!!.first()
        assertTrue(trackerLog.cookiesHasBeenBlocked)
        assertEquals("www.tracker.com", trackerLog.url)
        assertTrue(trackerLog.blockedCategories.contains(TrackingCategory.SCRIPTS_AND_SUB_RESOURCES))
        assertTrue(trackerLog.blockedCategories.contains(TrackingCategory.FINGERPRINTING))
        assertTrue(trackerLog.blockedCategories.contains(TrackingCategory.CRYPTOMINING))
        assertTrue(trackerLog.blockedCategories.contains(TrackingCategory.MOZILLA_SOCIAL))

        assertTrue(trackerLog.loadedCategories.contains(TrackingCategory.SCRIPTS_AND_SUB_RESOURCES))
        assertTrue(trackerLog.loadedCategories.contains(TrackingCategory.FINGERPRINTING))
        assertTrue(trackerLog.loadedCategories.contains(TrackingCategory.CRYPTOMINING))
        assertTrue(trackerLog.loadedCategories.contains(TrackingCategory.MOZILLA_SOCIAL))

        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)

        logEntriesResult = GeckoResult()
        whenever(mockContentBlockingController.getLog(any())).thenReturn(logEntriesResult)
        logEntriesResult.completeExceptionally(Exception())

        engine.getTrackersLog(
            mockSession,
            onSuccess = {
                trackersLog = it
                onSuccessCalled = true
            },
            onError = { onErrorCalled = true }
        )

        assertTrue(onErrorCalled)
    }

    @Test
    fun `fetch site with social trackers`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        val mockSession = mock<GeckoEngineSession>()
        val mockGeckoSetting = mock<GeckoRuntimeSettings>()
        val mockGeckoContentBlockingSetting = mock<ContentBlocking.Settings>()
        var trackersLog: List<TrackerLog>? = null

        val mockContentBlockingController = mock<ContentBlockingController>()
        var logEntriesResult = GeckoResult<List<ContentBlockingController.LogEntry>>()

        whenever(runtime.settings).thenReturn(mockGeckoSetting)
        whenever(mockGeckoSetting.contentBlocking).thenReturn(mockGeckoContentBlockingSetting)
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlockingController)
        whenever(mockContentBlockingController.getLog(any())).thenReturn(logEntriesResult)
        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.recommended()

        engine.getTrackersLog(mockSession, onSuccess = { trackersLog = it })
        logEntriesResult.complete(createSocialTrackersLogEntryList())

        var trackerLog = trackersLog!!.first()
        assertTrue(trackerLog.cookiesHasBeenBlocked)
        assertEquals("www.tracker.com", trackerLog.url)
        assertTrue(trackerLog.blockedCategories.contains(TrackingCategory.MOZILLA_SOCIAL))

        var trackerLog2 = trackersLog!![1]
        assertFalse(trackerLog2.cookiesHasBeenBlocked)
        assertEquals("www.tracker2.com", trackerLog2.url)
        assertTrue(trackerLog2.loadedCategories.contains(TrackingCategory.MOZILLA_SOCIAL))

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        logEntriesResult = GeckoResult()
        whenever(mockContentBlockingController.getLog(any())).thenReturn(logEntriesResult)

        engine.getTrackersLog(mockSession, onSuccess = { trackersLog = it })
        logEntriesResult.complete(createSocialTrackersLogEntryList())

        trackerLog = trackersLog!!.first()
        assertTrue(trackerLog.cookiesHasBeenBlocked)
        assertEquals("www.tracker.com", trackerLog.url)
        assertTrue(trackerLog.blockedCategories.contains(TrackingCategory.MOZILLA_SOCIAL))

        trackerLog2 = trackersLog!![1]
        assertFalse(trackerLog2.cookiesHasBeenBlocked)
        assertEquals("www.tracker2.com", trackerLog2.url)
        assertTrue(trackerLog2.loadedCategories.contains(TrackingCategory.MOZILLA_SOCIAL))
    }

    @Test
    fun `fetch trackers logged of the level 2 list`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        val mockSession = mock<GeckoEngineSession>()
        val mockGeckoSetting = mock<GeckoRuntimeSettings>()
        val mockGeckoContentBlockingSetting = mock<ContentBlocking.Settings>()
        var trackersLog: List<TrackerLog>? = null

        val mockContentBlockingController = mock<ContentBlockingController>()
        var logEntriesResult = GeckoResult<List<ContentBlockingController.LogEntry>>()

        whenever(runtime.settings).thenReturn(mockGeckoSetting)
        whenever(mockGeckoSetting.contentBlocking).thenReturn(mockGeckoContentBlockingSetting)
        whenever(mockGeckoContentBlockingSetting.getEnhancedTrackingProtectionLevel()).thenReturn(
            ContentBlocking.EtpLevel.STRICT
        )
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlockingController)
        whenever(mockContentBlockingController.getLog(any())).thenReturn(logEntriesResult)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.select(
            arrayOf(
                TrackingCategory.STRICT,
                TrackingCategory.CONTENT
            )
        )

        logEntriesResult = GeckoResult()
        whenever(runtime.contentBlockingController).thenReturn(mockContentBlockingController)
        whenever(mockContentBlockingController.getLog(any())).thenReturn(logEntriesResult)

        engine.getTrackersLog(
            mockSession,
            onSuccess = {
                trackersLog = it
            },
            onError = { }
        )
        logEntriesResult.complete(createDummyLogEntryList())

        val trackerLog = trackersLog!![1]
        assertTrue(trackerLog.loadedCategories.contains(TrackingCategory.SCRIPTS_AND_SUB_RESOURCES))
    }

    @Test
    fun `registerWebNotificationDelegate sets delegate`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)

        engine.registerWebNotificationDelegate(mock())

        verify(runtime).webNotificationDelegate = any()
    }

    @Test
    fun `registerWebPushDelegate sets delegate and returns same handler`() {
        val runtime = mock<GeckoRuntime>()
        val controller: WebPushController = mock()
        val engine = GeckoEngine(context, runtime = runtime)

        whenever(runtime.webPushController).thenReturn(controller)

        val handler1 = engine.registerWebPushDelegate(mock())
        val handler2 = engine.registerWebPushDelegate(mock())

        verify(controller, times(2)).setDelegate(any())

        assert(handler1 == handler2)
    }

    private fun createSocialTrackersLogEntryList(): List<ContentBlockingController.LogEntry> {
        val blockedLogEntry = object : ContentBlockingController.LogEntry() {}

        ReflectionUtils.setField(blockedLogEntry, "origin", "www.tracker.com")
        val blockedCookieSocialTracker = createBlockingData(Event.COOKIES_BLOCKED_SOCIALTRACKER)
        val blockedSocialContent = createBlockingData(Event.BLOCKED_SOCIALTRACKING_CONTENT)

        ReflectionUtils.setField(blockedLogEntry, "blockingData", listOf(blockedSocialContent, blockedCookieSocialTracker))

        val loadedLogEntry = object : ContentBlockingController.LogEntry() {}
        ReflectionUtils.setField(loadedLogEntry, "origin", "www.tracker2.com")

        val loadedCookieSocialTracker = createBlockingData(Event.COOKIES_LOADED_SOCIALTRACKER)
        val loadedSocialContent = createBlockingData(Event.LOADED_SOCIALTRACKING_CONTENT)

        ReflectionUtils.setField(loadedLogEntry, "blockingData", listOf(loadedCookieSocialTracker, loadedSocialContent))

        return listOf(blockedLogEntry, loadedLogEntry)
    }

    private fun createDummyLogEntryList(): List<ContentBlockingController.LogEntry> {
        val addLogEntry = object : ContentBlockingController.LogEntry() {}

        ReflectionUtils.setField(addLogEntry, "origin", "www.tracker.com")
        val blockedCookiePermission = createBlockingData(Event.COOKIES_BLOCKED_BY_PERMISSION)
        val loadedCookieSocialTracker = createBlockingData(Event.COOKIES_LOADED_SOCIALTRACKER)
        val blockedCookieSocialTracker = createBlockingData(Event.COOKIES_BLOCKED_SOCIALTRACKER)

        val blockedTrackingContent = createBlockingData(Event.BLOCKED_TRACKING_CONTENT)
        val blockedFingerprintingContent = createBlockingData(Event.BLOCKED_FINGERPRINTING_CONTENT)
        val blockedCyptominingContent = createBlockingData(Event.BLOCKED_CRYPTOMINING_CONTENT)
        val blockedSocialContent = createBlockingData(Event.BLOCKED_SOCIALTRACKING_CONTENT)

        val loadedTrackingLevel1Content = createBlockingData(Event.LOADED_LEVEL_1_TRACKING_CONTENT)
        val loadedTrackingLevel2Content = createBlockingData(Event.LOADED_LEVEL_2_TRACKING_CONTENT)
        val loadedFingerprintingContent = createBlockingData(Event.LOADED_FINGERPRINTING_CONTENT)
        val loadedCyptominingContent = createBlockingData(Event.LOADED_CRYPTOMINING_CONTENT)
        val loadedSocialContent = createBlockingData(Event.LOADED_SOCIALTRACKING_CONTENT)

        val contentBlockingList = listOf(
            blockedTrackingContent,
            loadedTrackingLevel1Content,
            loadedTrackingLevel2Content,
            blockedFingerprintingContent,
            loadedFingerprintingContent,
            blockedCyptominingContent,
            loadedCyptominingContent,
            blockedCookiePermission,
            blockedSocialContent,
            loadedSocialContent,
            loadedCookieSocialTracker,
            blockedCookieSocialTracker
        )

        val addLogSecondEntry = object : ContentBlockingController.LogEntry() {}
        ReflectionUtils.setField(addLogSecondEntry, "origin", "www.tracker2.com")
        val contentBlockingSecondEntryList = listOf(loadedTrackingLevel2Content)

        ReflectionUtils.setField(addLogEntry, "blockingData", contentBlockingList)
        ReflectionUtils.setField(addLogSecondEntry, "blockingData", contentBlockingSecondEntryList)

        return listOf(addLogEntry, addLogSecondEntry)
    }

    private fun createBlockingData(category: Int): ContentBlockingController.LogEntry.BlockingData {
        val blockingData = object : ContentBlockingController.LogEntry.BlockingData() {}
        ReflectionUtils.setField(blockingData, "category", category)
        return blockingData
    }

    private fun mockNativeExtension(useBundle: GeckoBundle? = null): GeckoWebExtension {
        val bundle = useBundle ?: GeckoBundle().apply {
            putString("webExtensionId", "id")
            putString("locationURI", "uri")
        }
        return spy(MockWebExtension(bundle))
    }

    private fun mockNativeExtension(id: String, location: String): GeckoWebExtension {
        val bundle = GeckoBundle().apply {
            putString("webExtensionId", id)
            putString("locationURI", location)
        }
        return spy(MockWebExtension(bundle))
    }
}