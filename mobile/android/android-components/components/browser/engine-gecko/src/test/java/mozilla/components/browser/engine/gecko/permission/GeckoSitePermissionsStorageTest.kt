/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.permission

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import mozilla.components.concept.engine.permission.SitePermissions.Status.NO_DECISION
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.test.ReflectionUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyBoolean
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_ALLOW
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_DENY
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission.VALUE_PROMPT
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_AUDIBLE
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_AUTOPLAY_INAUDIBLE
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_DESKTOP_NOTIFICATION
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_GEOLOCATION
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_MEDIA_KEY_SYSTEM_ACCESS
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_PERSISTENT_STORAGE
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_STORAGE_ACCESS
import org.mozilla.geckoview.GeckoSession.PermissionDelegate.PERMISSION_TRACKING
import org.mozilla.geckoview.StorageController
import org.mozilla.geckoview.StorageController.ClearFlags

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoSitePermissionsStorageTest {
    private lateinit var runtime: GeckoRuntime
    private lateinit var geckoStorage: GeckoSitePermissionsStorage
    private lateinit var onDiskStorage: SitePermissionsStorage
    private lateinit var storageController: StorageController

    @Before
    fun setup() {
        storageController = mock()
        runtime = mock()
        onDiskStorage = mock()

        whenever(runtime.storageController).thenReturn(storageController)

        geckoStorage = spy(GeckoSitePermissionsStorage(runtime, onDiskStorage))
    }

    @Test
    fun `GIVEN a location permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(location = ALLOWED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_GEOLOCATION)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_GEOLOCATION, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(NO_DECISION, permissionsCaptor.value.location)
        verify(storageController).setPermission(geckoPermissions, VALUE_ALLOW)
    }

    @Test
    fun `GIVEN a notification permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(notification = BLOCKED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_DESKTOP_NOTIFICATION)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_DESKTOP_NOTIFICATION, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(NO_DECISION, permissionsCaptor.value.notification)
        verify(storageController).setPermission(geckoPermissions, VALUE_DENY)
    }

    @Test
    fun `GIVEN a localStorage permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(localStorage = BLOCKED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_PERSISTENT_STORAGE)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_PERSISTENT_STORAGE, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(NO_DECISION, permissionsCaptor.value.localStorage)
        verify(storageController).setPermission(geckoPermissions, VALUE_DENY)
    }

    @Test
    fun `GIVEN a crossOriginStorageAccess permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(crossOriginStorageAccess = BLOCKED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_STORAGE_ACCESS)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_STORAGE_ACCESS, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(NO_DECISION, permissionsCaptor.value.crossOriginStorageAccess)
        verify(storageController).setPermission(geckoPermissions, VALUE_DENY)
    }

    @Test
    fun `GIVEN a mediaKeySystemAccess permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(mediaKeySystemAccess = ALLOWED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_MEDIA_KEY_SYSTEM_ACCESS)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_MEDIA_KEY_SYSTEM_ACCESS, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(NO_DECISION, permissionsCaptor.value.mediaKeySystemAccess)
        verify(storageController).setPermission(geckoPermissions, VALUE_ALLOW)
    }

    @Test
    fun `GIVEN a autoplayInaudible permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(autoplayInaudible = AutoplayStatus.ALLOWED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_AUTOPLAY_INAUDIBLE)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_AUTOPLAY_INAUDIBLE, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(AutoplayStatus.BLOCKED, permissionsCaptor.value.autoplayInaudible)
        verify(storageController).setPermission(geckoPermissions, VALUE_ALLOW)
    }

    @Test
    fun `GIVEN a autoplayAudible permission WHEN saving THEN the permission is saved in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(autoplayAudible = AutoplayStatus.ALLOWED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE, geckoPermissions, mock())
        val permissionsCaptor = argumentCaptor<SitePermissions>()

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(permissionsCaptor.capture(), any())

        assertEquals(AutoplayStatus.BLOCKED, permissionsCaptor.value.autoplayAudible)
        verify(storageController).setPermission(geckoPermissions, VALUE_ALLOW)
    }

    @Test
    fun `WHEN saving a site permission THEN the permission is saved in the gecko storage and in disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(autoplayAudible = AutoplayStatus.ALLOWED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE, geckoPermissions, mock())

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        geckoStorage.save(sitePermissions, geckoRequest)

        verify(onDiskStorage).save(sitePermissions.copy(autoplayAudible = AutoplayStatus.BLOCKED), geckoRequest)
        verify(storageController).setPermission(geckoPermissions, VALUE_ALLOW)
    }

    @Test
    fun `GIVEN a temporary permission WHEN saving THEN the permission is saved in memory`() = runTest {
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE, geckoPermissions, mock())

        geckoStorage.saveTemporary(geckoRequest)

        assertTrue(geckoStorage.geckoTemporaryPermissions.contains(geckoPermissions))
    }

    @Test
    fun `GIVEN media type temporary permission WHEN saving THEN the permission is NOT saved in memory`() = runTest {
        val geckoRequest = GeckoPermissionRequest.Media("mozilla.org", emptyList(), emptyList(), mock())

        assertTrue(geckoStorage.geckoTemporaryPermissions.isEmpty())

        geckoStorage.saveTemporary(geckoRequest)

        assertTrue(geckoStorage.geckoTemporaryPermissions.isEmpty())
    }

    @Test
    fun `GIVEN multiple saved temporary permissions WHEN clearing all temporary permission THEN all permissions are cleared`() = runTest {
        val geckoAutoPlayPermissions = geckoContentPermission("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE)
        val geckoPersistentStoragePermissions = geckoContentPermission("mozilla.org", PERMISSION_PERSISTENT_STORAGE)
        val geckoStorageAccessPermissions = geckoContentPermission("mozilla.org", PERMISSION_STORAGE_ACCESS)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE, geckoAutoPlayPermissions, mock())

        assertTrue(geckoStorage.geckoTemporaryPermissions.isEmpty())

        geckoStorage.saveTemporary(geckoRequest)

        assertEquals(1, geckoStorage.geckoTemporaryPermissions.size)

        geckoStorage.saveTemporary(geckoRequest.copy(geckoPermission = geckoPersistentStoragePermissions))

        assertEquals(2, geckoStorage.geckoTemporaryPermissions.size)

        geckoStorage.saveTemporary(geckoRequest.copy(geckoPermission = geckoStorageAccessPermissions))

        assertEquals(3, geckoStorage.geckoTemporaryPermissions.size)

        geckoStorage.clearTemporaryPermissions()

        verify(storageController).setPermission(geckoAutoPlayPermissions, VALUE_PROMPT)
        verify(storageController).setPermission(geckoPersistentStoragePermissions, VALUE_PROMPT)
        verify(storageController).setPermission(geckoStorageAccessPermissions, VALUE_PROMPT)

        assertTrue(geckoStorage.geckoTemporaryPermissions.isEmpty())
    }

    @Test
    fun `GIVEN a localStorage permission WHEN updating THEN the permission is updated in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(location = ALLOWED)
        val geckoPermissions = geckoContentPermission("mozilla.org", PERMISSION_GEOLOCATION)
        val geckoRequest = GeckoPermissionRequest.Content("mozilla.org", PERMISSION_AUTOPLAY_AUDIBLE, geckoPermissions, mock())

        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        val permission = geckoStorage.updateGeckoPermissionIfNeeded(sitePermissions, geckoRequest)

        assertEquals(NO_DECISION, permission.location)
        verify(storageController).setPermission(geckoPermissions, VALUE_ALLOW)
    }

    @Test
    fun `WHEN updating a permission THEN the permission is updated in the gecko storage and on the disk storage`() = runTest {
        val sitePermissions = createNewSitePermission().copy(location = ALLOWED)

        doReturn(sitePermissions).`when`(geckoStorage).updateGeckoPermissionIfNeeded(sitePermissions)

        geckoStorage.update(sitePermissions)

        verify(geckoStorage).updateGeckoPermissionIfNeeded(sitePermissions)
        verify(onDiskStorage).update(sitePermissions)
    }

    @Test
    fun `WHEN updating THEN the permission is updated in the gecko storage and set to the default value on the disk storage`() = runTest {
        val sitePermissions = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = ALLOWED,
            location = ALLOWED,
            notification = ALLOWED,
            microphone = ALLOWED,
            camera = ALLOWED,
            bluetooth = ALLOWED,
            mediaKeySystemAccess = ALLOWED,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            savedAt = 0,
        )
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS),
        )

        doReturn(geckoPermissions).`when`(geckoStorage).findGeckoContentPermissionBy(anyString(), anyBoolean())
        doReturn(Unit).`when`(geckoStorage).clearGeckoCacheFor(sitePermissions.origin)

        val permission = geckoStorage.updateGeckoPermissionIfNeeded(sitePermissions, null)

        geckoPermissions.forEach {
            verify(geckoStorage).removeTemporaryPermissionIfAny(it)
            verify(storageController).setPermission(it, VALUE_ALLOW)
        }

        assertEquals(NO_DECISION, permission.location)
        assertEquals(NO_DECISION, permission.notification)
        assertEquals(NO_DECISION, permission.localStorage)
        assertEquals(NO_DECISION, permission.crossOriginStorageAccess)
        assertEquals(NO_DECISION, permission.mediaKeySystemAccess)
        assertEquals(ALLOWED, permission.camera)
        assertEquals(ALLOWED, permission.microphone)
        assertEquals(AutoplayStatus.BLOCKED, permission.autoplayAudible)
        assertEquals(AutoplayStatus.BLOCKED, permission.autoplayInaudible)
    }

    @Test
    fun `WHEN querying the store by origin THEN the gecko and the on disk storage are queried and results are combined`() = runTest {
        val sitePermissions = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = ALLOWED,
            location = ALLOWED,
            notification = ALLOWED,
            microphone = ALLOWED,
            camera = ALLOWED,
            bluetooth = ALLOWED,
            mediaKeySystemAccess = ALLOWED,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            savedAt = 0,
        )
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION, value = VALUE_ALLOW),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION, value = VALUE_ALLOW),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS, value = VALUE_ALLOW),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE, value = VALUE_ALLOW),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS, value = VALUE_ALLOW),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE, value = VALUE_ALLOW),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE, value = VALUE_ALLOW),
        )

        doReturn(sitePermissions).`when`(onDiskStorage).findSitePermissionsBy("mozilla.dev", false)
        doReturn(geckoPermissions).`when`(geckoStorage).findGeckoContentPermissionBy("mozilla.dev", false)

        val foundPermissions = geckoStorage.findSitePermissionsBy("mozilla.dev", false)!!

        assertEquals(ALLOWED, foundPermissions.location)
        assertEquals(ALLOWED, foundPermissions.notification)
        assertEquals(ALLOWED, foundPermissions.localStorage)
        assertEquals(ALLOWED, foundPermissions.crossOriginStorageAccess)
        assertEquals(ALLOWED, foundPermissions.mediaKeySystemAccess)
        assertEquals(ALLOWED, foundPermissions.camera)
        assertEquals(ALLOWED, foundPermissions.microphone)
        assertEquals(AutoplayStatus.ALLOWED, foundPermissions.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, foundPermissions.autoplayInaudible)
    }

    @Test
    fun `GIVEN a gecko and on disk permissions WHEN merging values THEN both should be combined into one`() = runTest {
        val onDiskPermissions = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = ALLOWED,
            location = ALLOWED,
            notification = ALLOWED,
            microphone = ALLOWED,
            camera = ALLOWED,
            bluetooth = ALLOWED,
            mediaKeySystemAccess = ALLOWED,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            savedAt = 0,
        )
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE, value = VALUE_DENY),
        ).groupByType()

        val mergedPermissions = geckoStorage.mergePermissions(onDiskPermissions, geckoPermissions)!!

        assertEquals(BLOCKED, mergedPermissions.location)
        assertEquals(BLOCKED, mergedPermissions.notification)
        assertEquals(BLOCKED, mergedPermissions.localStorage)
        assertEquals(BLOCKED, mergedPermissions.crossOriginStorageAccess)
        assertEquals(BLOCKED, mergedPermissions.mediaKeySystemAccess)
        assertEquals(ALLOWED, mergedPermissions.camera)
        assertEquals(ALLOWED, mergedPermissions.microphone)
        assertEquals(AutoplayStatus.BLOCKED, mergedPermissions.autoplayAudible)
        assertEquals(AutoplayStatus.BLOCKED, mergedPermissions.autoplayInaudible)
    }

    @Test
    fun `GIVEN permissions that are not present on the gecko storage WHEN merging THEN favor the values on disk permissions`() = runTest {
        val onDiskPermissions = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = ALLOWED,
            location = ALLOWED,
            notification = ALLOWED,
            microphone = ALLOWED,
            camera = ALLOWED,
            bluetooth = ALLOWED,
            mediaKeySystemAccess = ALLOWED,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            savedAt = 0,
        )
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION, value = VALUE_DENY),
        ).groupByType()

        val mergedPermissions = geckoStorage.mergePermissions(onDiskPermissions, geckoPermissions)!!

        assertEquals(BLOCKED, mergedPermissions.location)
        assertEquals(ALLOWED, mergedPermissions.notification)
        assertEquals(ALLOWED, mergedPermissions.localStorage)
        assertEquals(ALLOWED, mergedPermissions.crossOriginStorageAccess)
        assertEquals(ALLOWED, mergedPermissions.mediaKeySystemAccess)
        assertEquals(ALLOWED, mergedPermissions.camera)
        assertEquals(ALLOWED, mergedPermissions.microphone)
        assertEquals(AutoplayStatus.ALLOWED, mergedPermissions.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, mergedPermissions.autoplayInaudible)
    }

    @Test
    fun `GIVEN different cross_origin_storage_access permissions WHEN mergePermissions is called THEN they are filtered by origin url`() {
        val onDiskPermissions = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = NO_DECISION,
            location = ALLOWED,
            notification = ALLOWED,
            microphone = ALLOWED,
            camera = ALLOWED,
            bluetooth = ALLOWED,
            mediaKeySystemAccess = ALLOWED,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            savedAt = 0,
        )
        val geckoPermission1 = geckoContentPermission(
            type = PERMISSION_STORAGE_ACCESS,
            value = VALUE_DENY,
            thirdPartyOrigin = "mozilla.com",
        )
        val geckoPermission2 = geckoContentPermission(
            type = PERMISSION_STORAGE_ACCESS,
            value = VALUE_ALLOW,
            thirdPartyOrigin = "mozilla.dev",
        )
        val geckoPermission3 = geckoContentPermission(
            type = PERMISSION_STORAGE_ACCESS,
            value = VALUE_PROMPT,
            thirdPartyOrigin = "mozilla.org",
        )

        val mergedPermissions = geckoStorage.mergePermissions(
            onDiskPermissions,
            mapOf(PERMISSION_STORAGE_ACCESS to listOf(geckoPermission1, geckoPermission2, geckoPermission3)),
        )

        assertEquals(onDiskPermissions.copy(crossOriginStorageAccess = ALLOWED), mergedPermissions!!)
    }

    @Test
    fun `WHEN removing a site permissions THEN permissions should be removed from the on disk and gecko storage`() = runTest {
        val onDiskPermissions = createNewSitePermission()

        doReturn(Unit).`when`(geckoStorage).removeGeckoContentPermissionBy(onDiskPermissions.origin)

        geckoStorage.remove(onDiskPermissions)

        verify(onDiskStorage).remove(onDiskPermissions)
        verify(geckoStorage).removeGeckoContentPermissionBy(onDiskPermissions.origin)
    }

    @Test
    fun `WHEN removing gecko permissions THEN permissions should be set to the default values in the gecko storage`() = runTest {
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE),
            geckoContentPermission(type = PERMISSION_TRACKING),
        )

        doReturn(geckoPermissions).`when`(geckoStorage).findGeckoContentPermissionBy(anyString(), anyBoolean())

        geckoStorage.removeGeckoContentPermissionBy("mozilla.dev")

        geckoPermissions.forEach {
            val value = if (it.permission != PERMISSION_TRACKING) {
                VALUE_PROMPT
            } else {
                VALUE_DENY
            }
            verify(geckoStorage).removeTemporaryPermissionIfAny(it)
            verify(storageController).setPermission(it, value)
        }
    }

    @Test
    fun `WHEN removing a temporary permissions THEN the permissions should be remove from memory`() = runTest {
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE),
            geckoContentPermission(type = PERMISSION_TRACKING),
        )

        assertTrue(geckoStorage.geckoTemporaryPermissions.isEmpty())

        geckoStorage.geckoTemporaryPermissions.addAll(geckoPermissions)

        assertEquals(10, geckoStorage.geckoTemporaryPermissions.size)

        geckoPermissions.forEach {
            geckoStorage.removeTemporaryPermissionIfAny(it)
        }

        assertTrue(geckoStorage.geckoTemporaryPermissions.isEmpty())
    }

    @Test
    fun `WHEN removing all THEN all permissions should be removed from the on disk and gecko storage`() = runTest {
        doReturn(Unit).`when`(geckoStorage).removeGeckoAllContentPermissions()

        geckoStorage.removeAll()

        verify(onDiskStorage).removeAll()
        verify(geckoStorage).removeGeckoAllContentPermissions()
    }

    @Test
    fun `WHEN removing all gecko permissions THEN remove all permissions on gecko and clear the site permissions info`() = runTest {
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE),
            geckoContentPermission(type = PERMISSION_TRACKING),
        )

        doReturn(geckoPermissions).`when`(geckoStorage).findAllGeckoContentPermissions()
        doNothing().`when`(geckoStorage).removeGeckoContentPermission(any())

        geckoStorage.removeGeckoAllContentPermissions()

        geckoPermissions.forEach {
            verify(geckoStorage).removeGeckoContentPermission(it)
        }
        verify(storageController).clearData(ClearFlags.PERMISSIONS)
    }

    @Test
    fun `WHEN querying all permission THEN the gecko and the on disk storage are queried and results are combined`() = runTest {
        val onDiskPermissions = SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = ALLOWED,
            location = ALLOWED,
            notification = ALLOWED,
            microphone = ALLOWED,
            camera = ALLOWED,
            bluetooth = ALLOWED,
            mediaKeySystemAccess = ALLOWED,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED,
            savedAt = 0,
        )
        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE, value = VALUE_DENY),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE, value = VALUE_DENY),
        )

        doReturn(listOf(onDiskPermissions)).`when`(onDiskStorage).all()
        doReturn(geckoPermissions).`when`(geckoStorage).findAllGeckoContentPermissions()

        val foundPermissions = geckoStorage.all().first()

        assertEquals(BLOCKED, foundPermissions.location)
        assertEquals(BLOCKED, foundPermissions.notification)
        assertEquals(BLOCKED, foundPermissions.localStorage)
        assertEquals(BLOCKED, foundPermissions.crossOriginStorageAccess)
        assertEquals(BLOCKED, foundPermissions.mediaKeySystemAccess)
        assertEquals(ALLOWED, foundPermissions.camera)
        assertEquals(ALLOWED, foundPermissions.microphone)
        assertEquals(AutoplayStatus.BLOCKED, foundPermissions.autoplayAudible)
        assertEquals(AutoplayStatus.BLOCKED, foundPermissions.autoplayInaudible)
    }

    @Test
    fun `WHEN filtering temporary permissions THEN all temporary permissions should be removed`() = runTest {
        val temporary = listOf(geckoContentPermission(type = PERMISSION_GEOLOCATION))

        val geckoPermissions = listOf(
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_GEOLOCATION),
            geckoContentPermission(type = PERMISSION_DESKTOP_NOTIFICATION),
            geckoContentPermission(type = PERMISSION_MEDIA_KEY_SYSTEM_ACCESS),
            geckoContentPermission(type = PERMISSION_PERSISTENT_STORAGE),
            geckoContentPermission(type = PERMISSION_STORAGE_ACCESS),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_AUDIBLE),
            geckoContentPermission(type = PERMISSION_AUTOPLAY_INAUDIBLE),
        )

        val filteredPermissions = geckoPermissions.filterNotTemporaryPermissions(temporary)!!

        assertEquals(6, filteredPermissions.size)
        assertFalse(filteredPermissions.any { it.permission == PERMISSION_GEOLOCATION })
    }

    @Test
    fun `WHEN compering two gecko ContentPermissions THEN they are the same when host and permissions are the same`() = runTest {
        val location1 = geckoContentPermission(uri = "mozilla.dev", type = PERMISSION_GEOLOCATION)
        val location2 = geckoContentPermission(uri = "mozilla.dev", type = PERMISSION_GEOLOCATION)
        val notification = geckoContentPermission(uri = "mozilla.dev", type = PERMISSION_DESKTOP_NOTIFICATION)

        assertTrue(location1.areSame(location2))
        assertFalse(notification.areSame(location1))
    }

    @Test
    fun `WHEN converting from gecko status to sitePermissions status THEN they get converted to the equivalent one`() = runTest {
        assertEquals(NO_DECISION, VALUE_PROMPT.toStatus())
        assertEquals(BLOCKED, VALUE_DENY.toStatus())
        assertEquals(ALLOWED, VALUE_ALLOW.toStatus())
    }

    @Test
    fun `WHEN converting from gecko status to autoplay sitePermissions status THEN they get converted to the equivalent one`() = runTest {
        assertEquals(AutoplayStatus.BLOCKED, VALUE_PROMPT.toAutoPlayStatus())
        assertEquals(AutoplayStatus.BLOCKED, VALUE_DENY.toAutoPlayStatus())
        assertEquals(AutoplayStatus.ALLOWED, VALUE_ALLOW.toAutoPlayStatus())
    }

    @Test
    fun `WHEN converting a sitePermissions status to gecko status THEN they get converted to the equivalent one`() = runTest {
        assertEquals(VALUE_PROMPT, NO_DECISION.toGeckoStatus())
        assertEquals(VALUE_DENY, BLOCKED.toGeckoStatus())
        assertEquals(VALUE_ALLOW, ALLOWED.toGeckoStatus())
    }

    @Test
    fun `WHEN converting from autoplay sitePermissions to gecko status THEN they get converted to the equivalent one`() = runTest {
        assertEquals(VALUE_DENY, AutoplayStatus.BLOCKED.toGeckoStatus())
        assertEquals(VALUE_ALLOW, AutoplayStatus.ALLOWED.toGeckoStatus())
    }

    private fun createNewSitePermission(): SitePermissions {
        return SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            crossOriginStorageAccess = BLOCKED,
            location = BLOCKED,
            notification = NO_DECISION,
            microphone = NO_DECISION,
            camera = NO_DECISION,
            bluetooth = ALLOWED,
            savedAt = 0,
        )
    }
}

internal fun geckoContentPermission(
    uri: String = "mozilla.dev",
    type: Int,
    value: Int = VALUE_PROMPT,
    thirdPartyOrigin: String = "mozilla.dev",
): ContentPermission {
    val prompt: ContentPermission = mock()
    ReflectionUtils.setField(prompt, "uri", uri)
    ReflectionUtils.setField(prompt, "thirdPartyOrigin", thirdPartyOrigin)
    ReflectionUtils.setField(prompt, "permission", type)
    ReflectionUtils.setField(prompt, "value", value)
    return prompt
}
