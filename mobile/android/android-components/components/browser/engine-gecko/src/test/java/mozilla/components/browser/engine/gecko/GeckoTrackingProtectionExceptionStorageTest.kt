/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko

import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import mozilla.components.browser.engine.gecko.content.blocking.GeckoTrackingProtectionException
import mozilla.components.browser.engine.gecko.permission.geckoContentPermission
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_TRACKING
import org.mozilla.geckoview.StorageController
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class GeckoTrackingProtectionExceptionStorageTest {

    private lateinit var runtime: GeckoRuntime

    private lateinit var storage: GeckoTrackingProtectionExceptionStorage

    @Before
    fun setup() {
        runtime = mock()
        whenever(runtime.settings).thenReturn(mock())
        storage = spy(GeckoTrackingProtectionExceptionStorage(runtime))
        storage.scope = CoroutineScope(Dispatchers.Main)
    }

    @Test
    fun `GIVEN a new exception WHEN adding THEN the exception is stored on the gecko storage`() {
        val storageController = mock<StorageController>()
        val mockGeckoSession = mock<GeckoSession>()
        val session = spy(GeckoEngineSession(runtime, geckoSessionProvider = { mockGeckoSession }))

        val geckoPermission = geckoContentPermission(type = PERMISSION_TRACKING, value = VALUE_ALLOW)
        session.geckoPermissions = listOf(geckoPermission)

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.storageController).thenReturn(storageController)

        var excludedOnTrackingProtection = false

        session.register(
            object : EngineSession.Observer {
                override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
                    excludedOnTrackingProtection = excluded
                }
            },
        )

        storage.add(session)

        verify(storageController).setPermission(geckoPermission, VALUE_ALLOW)
        assertTrue(excludedOnTrackingProtection)
    }

    @Test
    fun `GIVEN a persistInPrivateMode new exception WHEN adding THEN the exception is stored on the gecko storage`() {
        val storageController = mock<StorageController>()
        val mockGeckoSession = mock<GeckoSession>()
        val session = spy(GeckoEngineSession(runtime, geckoSessionProvider = { mockGeckoSession }))

        val geckoPermission = geckoContentPermission(type = PERMISSION_TRACKING, value = VALUE_ALLOW)
        session.geckoPermissions = listOf(geckoPermission)

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(runtime.storageController).thenReturn(storageController)

        var excludedOnTrackingProtection = false

        session.register(
            object : EngineSession.Observer {
                override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
                    excludedOnTrackingProtection = excluded
                }
            },
        )

        storage.add(session, persistInPrivateMode = true)

        verify(storageController).setPrivateBrowsingPermanentPermission(geckoPermission, VALUE_ALLOW)
        assertTrue(excludedOnTrackingProtection)
    }

    @Test
    fun `WHEN removing an exception by session THEN the session is removed of the exception list`() {
        val mockGeckoSession = mock<GeckoSession>()
        val session = spy(GeckoEngineSession(runtime, geckoSessionProvider = { mockGeckoSession }))

        whenever(session.geckoSession).thenReturn(mockGeckoSession)
        whenever(session.currentUrl).thenReturn("https://example.com/")
        doNothing().`when`(storage).remove(anyString())

        var excludedOnTrackingProtection = true

        session.register(
            object : EngineSession.Observer {
                override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
                    excludedOnTrackingProtection = excluded
                }
            },
        )

        storage.remove(session)

        verify(storage).remove(anyString())
        assertFalse(excludedOnTrackingProtection)
    }

    @Test
    fun `GIVEN TrackingProtectionException WHEN removing THEN remove the exception using with its contentPermission`() {
        val geckoException = mock<GeckoTrackingProtectionException>()
        val contentPermission = mock<ContentPermission>()

        whenever(geckoException.contentPermission).thenReturn(contentPermission)
        doNothing().`when`(storage).remove(contentPermission)

        storage.remove(geckoException)
        verify(storage).remove(geckoException.contentPermission)
    }

    @Test
    fun `GIVEN URL WHEN removing THEN remove the exception using with its URL`() {
        val exception = mock<TrackingProtectionException>()

        whenever(exception.url).thenReturn("https://example.com/")
        doNothing().`when`(storage).remove(anyString())

        storage.remove(exception)
        verify(storage).remove(anyString())
    }

    @Test
    fun `WHEN removing an exception by contentPermission THEN remove it from the gecko storage`() {
        val contentPermission = mock<ContentPermission>()
        val storageController = mock<StorageController>()

        whenever(runtime.storageController).thenReturn(storageController)

        storage.remove(contentPermission)

        verify(storageController).setPermission(contentPermission, VALUE_DENY)
    }

    @Test
    fun `WHEN removing an exception by URL THEN try to find it in the gecko store and remove it`() {
        val contentPermission =
            geckoContentPermission("https://example.com/", PERMISSION_TRACKING, VALUE_ALLOW)
        val storageController = mock<StorageController>()
        val geckoResult = GeckoResult<List<ContentPermission>>()

        whenever(runtime.storageController).thenReturn(storageController)
        whenever(runtime.storageController.allPermissions).thenReturn(geckoResult)

        storage.remove("https://example.com/")

        geckoResult.complete(listOf(contentPermission))
        shadowOf(getMainLooper()).idle()

        verify(storageController).setPermission(contentPermission, VALUE_DENY)
    }

    @Test
    fun `WHEN removing all exceptions THEN remove all the exceptions in the gecko store`() {
        val mockGeckoSession = mock<GeckoSession>()
        val session = GeckoEngineSession(runtime, geckoSessionProvider = { mockGeckoSession })

        val contentPermission =
            geckoContentPermission("https://example.com/", PERMISSION_TRACKING, VALUE_ALLOW)
        val storageController = mock<StorageController>()
        val geckoResult = GeckoResult<List<ContentPermission>>()
        var excludedOnTrackingProtection = true

        session.register(
            object : EngineSession.Observer {
                override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
                    excludedOnTrackingProtection = excluded
                }
            },
        )

        whenever(runtime.storageController).thenReturn(storageController)
        whenever(runtime.storageController.allPermissions).thenReturn(geckoResult)

        storage.removeAll(listOf(session))

        geckoResult.complete(listOf(contentPermission))
        shadowOf(getMainLooper()).idle()

        verify(storageController).setPermission(contentPermission, VALUE_DENY)
        assertFalse(excludedOnTrackingProtection)
    }

    @Test
    fun `WHEN querying all exceptions THEN all the exceptions in the gecko store should be fetched`() {
        val contentPermission =
            geckoContentPermission("https://example.com/", PERMISSION_TRACKING, VALUE_ALLOW)
        val storageController = mock<StorageController>()
        val geckoResult = GeckoResult<List<ContentPermission>>()
        var exceptionList: List<TrackingProtectionException>? = null

        whenever(runtime.storageController).thenReturn(storageController)
        whenever(runtime.storageController.allPermissions).thenReturn(geckoResult)

        storage.fetchAll { exceptions ->
            exceptionList = exceptions
        }

        geckoResult.complete(listOf(contentPermission))
        shadowOf(getMainLooper()).idle()

        assertTrue(exceptionList!!.isNotEmpty())
        val exception = exceptionList!!.first() as GeckoTrackingProtectionException

        assertEquals("https://example.com/", exception.url)
        assertEquals(contentPermission, exception.contentPermission)
    }

    @Test
    fun `WHEN checking if exception is on the exception list THEN the exception is found in the storage`() {
        val session = mock<GeckoEngineSession>()
        val mockGeckoSession = mock<GeckoSession>()
        var containsException = false
        val contentPermission =
            geckoContentPermission("https://example.com/", PERMISSION_TRACKING, VALUE_ALLOW)
        val storageController = mock<StorageController>()
        val geckoResult = GeckoResult<List<ContentPermission>>()

        whenever(runtime.storageController).thenReturn(storageController)
        whenever(runtime.storageController.allPermissions).thenReturn(geckoResult)

        whenever(session.currentUrl).thenReturn("https://example.com/")
        whenever(session.geckoSession).thenReturn(mockGeckoSession)

        storage.contains(session) { contains ->
            containsException = contains
        }

        geckoResult.complete(listOf(contentPermission))
        shadowOf(getMainLooper()).idle()

        assertTrue(containsException)

        whenever(session.currentUrl).thenReturn("")

        storage.contains(session) { contains ->
            containsException = contains
        }

        assertFalse(containsException)
    }
}
