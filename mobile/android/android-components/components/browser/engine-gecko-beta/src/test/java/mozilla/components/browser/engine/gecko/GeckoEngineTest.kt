/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.app.Activity
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.engine.gecko.mediaquery.toGeckoValue
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.engine.EngineSession.SafeBrowsingPolicy
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy.CookiePolicy
import mozilla.components.concept.engine.UnsupportedSettingException
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.ContentBlockingController
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.StorageController
import org.robolectric.Robolectric
import java.io.IOException
import java.lang.Exception
import org.mozilla.geckoview.WebExtension as GeckoWebExtension

@RunWith(AndroidJUnit4::class)
class GeckoEngineTest {

    private val runtime: GeckoRuntime = mock()
    private val context: Context = mock()

    @Test
    fun createView() {
        assertTrue(GeckoEngine(context, runtime = runtime).createView(
            Robolectric.buildActivity(Activity::class.java).get()
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
        val contentBlockingSettings = ContentBlocking.Settings.Builder().build()
        val runtime = mock<GeckoRuntime>()
        val runtimeSettings = mock<GeckoRuntimeSettings>()
        whenever(runtimeSettings.javaScriptEnabled).thenReturn(true)
        whenever(runtimeSettings.webFontsEnabled).thenReturn(true)
        whenever(runtimeSettings.automaticFontSizeAdjustment).thenReturn(true)
        whenever(runtimeSettings.fontInflationEnabled).thenReturn(true)
        whenever(runtimeSettings.fontSizeFactor).thenReturn(1.0F)
        whenever(runtimeSettings.contentBlocking).thenReturn(contentBlockingSettings)
        whenever(runtimeSettings.preferredColorScheme).thenReturn(GeckoRuntimeSettings.COLOR_SCHEME_SYSTEM)
        whenever(runtimeSettings.autoplayDefault).thenReturn(GeckoRuntimeSettings.AUTOPLAY_DEFAULT_ALLOWED)
        whenever(runtime.settings).thenReturn(runtimeSettings)
        val engine = GeckoEngine(context, runtime = runtime, defaultSettings = defaultSettings)

        assertTrue(engine.settings.javascriptEnabled)
        engine.settings.javascriptEnabled = false
        verify(runtimeSettings).javaScriptEnabled = false

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

        assertFalse(engine.settings.remoteDebuggingEnabled)
        engine.settings.remoteDebuggingEnabled = true
        verify(runtimeSettings).remoteDebuggingEnabled = true

        assertFalse(engine.settings.testingModeEnabled)
        engine.settings.testingModeEnabled = true
        assertTrue(engine.settings.testingModeEnabled)

        assertEquals(PreferredColorScheme.System, engine.settings.preferredColorScheme)
        engine.settings.preferredColorScheme = PreferredColorScheme.Dark
        verify(runtimeSettings).preferredColorScheme = PreferredColorScheme.Dark.toGeckoValue()

        assertTrue(engine.settings.allowAutoplayMedia)
        engine.settings.allowAutoplayMedia = false
        verify(runtimeSettings).autoplayDefault = GeckoRuntimeSettings.AUTOPLAY_DEFAULT_BLOCKED

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
            TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id
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

        val geckoRunTime = GeckoRuntime.getDefault(testContext)

        val engine = GeckoEngine(testContext, runtime = geckoRunTime)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.strict()

        val trackingStrictCategories = TrackingProtectionPolicy.strict().trackingCategories.sumBy { it.id }
        val artificialCategory =
            TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id

        assertEquals(
            trackingStrictCategories - artificialCategory,
            geckoRunTime.settings.contentBlocking.antiTrackingCategories
        )

        geckoRunTime.settings.contentBlocking.setAntiTracking(0)

        engine.settings.trackingProtectionPolicy = TrackingProtectionPolicy.select(
            arrayOf(TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES)
        )

        assertEquals(0, geckoRunTime.settings.contentBlocking.antiTrackingCategories)
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
    fun `WHEN a non strict tracking protection policy is set THEN the strict social list must be disabled`() {
        val mockRuntime = mock<GeckoRuntime>()
        whenever(mockRuntime.settings).thenReturn(mock())
        whenever(mockRuntime.settings.contentBlocking).thenReturn(mock())

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
        whenever(runtimeSettings.autoplayDefault).thenReturn(GeckoRuntimeSettings.AUTOPLAY_DEFAULT_BLOCKED)
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
                allowAutoplayMedia = false,
                suspendMediaWhenInactive = true
            ), runtime)

        verify(runtimeSettings).javaScriptEnabled = false
        verify(runtimeSettings).webFontsEnabled = false
        verify(runtimeSettings).automaticFontSizeAdjustment = false
        verify(runtimeSettings).fontInflationEnabled = false
        verify(runtimeSettings).fontSizeFactor = 2.0F
        verify(runtimeSettings).remoteDebuggingEnabled = true
        verify(runtimeSettings).autoplayDefault = GeckoRuntimeSettings.AUTOPLAY_DEFAULT_BLOCKED

        val trackingStrictCategories = TrackingProtectionPolicy.strict().trackingCategories.sumBy { it.id }
        val artificialCategory =
            TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES.id
        assertEquals(
            trackingStrictCategories - artificialCategory,
            contentBlockingSettings.antiTrackingCategories
        )

        assertEquals(SafeBrowsingPolicy.RECOMMENDED.id, contentBlockingSettings.safeBrowsingCategories)

        assertEquals(CookiePolicy.ACCEPT_NON_TRACKERS.id, contentBlockingSettings.cookieBehavior)
        assertTrue(engine.settings.testingModeEnabled)
        assertEquals("test-ua", engine.settings.userAgentString)
        assertEquals(PreferredColorScheme.Light, engine.settings.preferredColorScheme)
        assertFalse(engine.settings.allowAutoplayMedia)
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
    fun `fetch trackers logged successfully`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        val mockSession = mock<GeckoEngineSession>()
        var trackersLog: List<TrackerLog>? = null

        val mockContentBlockingController = mock<ContentBlockingController>()
        var logEntriesResult = GeckoResult<List<ContentBlockingController.LogEntry>>()

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
        assertTrue(trackerLog.blockedCategories.contains(TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES))
        assertTrue(trackerLog.blockedCategories.contains(TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING))
        assertTrue(trackerLog.blockedCategories.contains(TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING))
        assertTrue(trackerLog.blockedCategories.contains(TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL))

        assertTrue(trackerLog.loadedCategories.contains(TrackingProtectionPolicy.TrackingCategory.SCRIPTS_AND_SUB_RESOURCES))
        assertTrue(trackerLog.loadedCategories.contains(TrackingProtectionPolicy.TrackingCategory.FINGERPRINTING))
        assertTrue(trackerLog.loadedCategories.contains(TrackingProtectionPolicy.TrackingCategory.CRYPTOMINING))
        assertTrue(trackerLog.loadedCategories.contains(TrackingProtectionPolicy.TrackingCategory.MOZILLA_SOCIAL))

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
    fun `install web extension successfully`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        val result = GeckoResult<Void>()

        whenever(runtime.registerWebExtension(any())).thenReturn(result)
        engine.installWebExtension(
                "test-webext",
                "resource://android/assets/extensions/test",
                onSuccess = { onSuccessCalled = true },
                onError = { _, _ -> onErrorCalled = true }
        )
        result.complete(null)

        val extCaptor = argumentCaptor<GeckoWebExtension>()
        verify(runtime).registerWebExtension(extCaptor.capture())
        assertEquals("test-webext", extCaptor.value.id)
        assertEquals("resource://android/assets/extensions/test", extCaptor.value.location)
        assertEquals(GeckoWebExtension.Flags.ALLOW_CONTENT_MESSAGING, extCaptor.value.flags)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `install web extension successfully but do not allow content messaging`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        var onSuccessCalled = false
        var onErrorCalled = false
        val result = GeckoResult<Void>()

        whenever(runtime.registerWebExtension(any())).thenReturn(result)
        engine.installWebExtension(
                "test-webext",
                "resource://android/assets/extensions/test",
                allowContentMessaging = false,
                onSuccess = { onSuccessCalled = true },
                onError = { _, _ -> onErrorCalled = true }
        )
        result.complete(null)

        val extCaptor = argumentCaptor<GeckoWebExtension>()
        verify(runtime).registerWebExtension(extCaptor.capture())
        assertEquals("test-webext", extCaptor.value.id)
        assertEquals("resource://android/assets/extensions/test", extCaptor.value.location)
        assertEquals(GeckoWebExtension.Flags.NONE, extCaptor.value.flags)
        assertTrue(onSuccessCalled)
        assertFalse(onErrorCalled)
    }

    @Test
    fun `install web extension failure`() {
        val runtime = mock<GeckoRuntime>()
        val engine = GeckoEngine(context, runtime = runtime)
        var onErrorCalled = false
        val expected = IOException()
        val result = GeckoResult<Void>()

        var throwable: Throwable? = null
        whenever(runtime.registerWebExtension(any())).thenReturn(result)
        engine.installWebExtension("test-webext-error", "resource://android/assets/extensions/error") { _, e ->
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

    private fun createDummyLogEntryList(): List<ContentBlockingController.LogEntry> {
        val addLogEntry = object : ContentBlockingController.LogEntry() {}

        ReflectionUtils.setField(addLogEntry, "origin", "www.tracker.com")
        val blockedCookiePermission = createBlockingData(ContentBlockingController.Event.COOKIES_BLOCKED_BY_PERMISSION)

        val blockedTrackingContent = createBlockingData(ContentBlockingController.Event.BLOCKED_TRACKING_CONTENT)
        val blockedFingerprintingContent = createBlockingData(ContentBlockingController.Event.BLOCKED_FINGERPRINTING_CONTENT)
        val blockedCyptominingContent = createBlockingData(ContentBlockingController.Event.BLOCKED_CRYPTOMINING_CONTENT)
        val blockedSocialContent = createBlockingData(ContentBlockingController.Event.BLOCKED_SOCIALTRACKING_CONTENT)

        val loadedTrackingContent = createBlockingData(ContentBlockingController.Event.LOADED_TRACKING_CONTENT)
        val loadedFingerprintingContent = createBlockingData(ContentBlockingController.Event.LOADED_FINGERPRINTING_CONTENT)
        val loadedCyptominingContent = createBlockingData(ContentBlockingController.Event.LOADED_CRYPTOMINING_CONTENT)
        val loadedSocialContent = createBlockingData(ContentBlockingController.Event.LOADED_SOCIALTRACKING_CONTENT)

        val contentBlockingList = listOf(
            blockedTrackingContent,
            loadedTrackingContent,
            blockedFingerprintingContent,
            loadedFingerprintingContent,
            blockedCyptominingContent,
            loadedCyptominingContent,
            blockedCookiePermission,
            blockedSocialContent,
            loadedSocialContent
        )

        ReflectionUtils.setField(addLogEntry, "blockingData", contentBlockingList)

        return listOf(addLogEntry)
    }

    private fun createBlockingData(category: Int): ContentBlockingController.LogEntry.BlockingData {
        val blockingData = object : ContentBlockingController.LogEntry.BlockingData() {}
        ReflectionUtils.setField(blockingData, "category", category)
        return blockingData
    }
}