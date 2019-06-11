/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions

import androidx.paging.DataSource
import androidx.room.DatabaseConfiguration
import androidx.room.InvalidationTracker
import androidx.sqlite.db.SupportSQLiteOpenHelper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.sitepermissions.SitePermissions.Status.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissions.Status.NO_DECISION
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.BLUETOOTH
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.CAMERA
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.LOCAL_STORAGE
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.LOCATION
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.MICROPHONE
import mozilla.components.feature.sitepermissions.SitePermissionsStorage.Permission.NOTIFICATION
import mozilla.components.feature.sitepermissions.db.SitePermissionsDao
import mozilla.components.feature.sitepermissions.db.SitePermissionsDatabase
import mozilla.components.feature.sitepermissions.db.SitePermissionsEntity
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SitePermissionsStorageTest {

    private lateinit var mockDAO: SitePermissionsDao
    private lateinit var storage: SitePermissionsStorage

    @Before
    fun setup() {
        mockDAO = mock()
        storage = spy(SitePermissionsStorage(mock()).apply {
            databaseInitializer = { mockDatabase(mockDAO) }
        })
    }

    @Test
    fun `save a new SitePermission`() {
        val sitePermissions = createNewSitePermission()

        storage.save(sitePermissions)

        verify(mockDAO).insert(any())
    }

    @Test
    fun `update a SitePermission`() {
        val sitePermissions = createNewSitePermission()

        storage.update(sitePermissions)

        verify(mockDAO).update(any())
    }

    @Test
    fun `find a SitePermissions by origin`() {
        storage.findSitePermissionsBy("mozilla.org")

        verify(mockDAO).getSitePermissionsBy("mozilla.org")
    }

    @Test
    fun `find all sitePermissions grouped by permission`() {
        doReturn(dummySitePermissionEntitiesList())
            .`when`(mockDAO).getSitePermissions()

        val map = storage.findAllSitePermissionsGroupedByPermission()

        verify(mockDAO).getSitePermissions()

        assertEquals(2, map[LOCAL_STORAGE]?.size)
        assertFalse(LOCATION in map)
        assertFalse(NOTIFICATION in map)
        assertFalse(CAMERA in map)
        assertEquals(2, map[BLUETOOTH]?.size)
        assertEquals(2, map[MICROPHONE]?.size)
    }

    @Test
    fun `remove a SitePermissions`() {
        val sitePermissions = createNewSitePermission()

        storage.remove(sitePermissions)

        verify(mockDAO).deleteSitePermissions(any())
    }

    @Test
    fun `remove all SitePermissions`() {
        storage.removeAll()

        verify(mockDAO).deleteAllSitePermissions()
    }

    @Test
    fun `get all SitePermissions paged`() {
        val mockDataSource: DataSource<Int, SitePermissionsEntity> = mock()

        doReturn(object : DataSource.Factory<Int, SitePermissionsEntity>() {
            override fun create(): DataSource<Int, SitePermissionsEntity> {
                return mockDataSource
            }
        }).`when`(mockDAO).getSitePermissionsPaged()

        storage.getSitePermissionsPaged()

        verify(mockDAO).getSitePermissionsPaged()
    }

    private fun createNewSitePermission(): SitePermissions {
        return SitePermissions(
            origin = "mozilla.dev",
            localStorage = ALLOWED,
            location = BLOCKED,
            notification = NO_DECISION,
            microphone = NO_DECISION,
            camera = NO_DECISION,
            bluetooth = ALLOWED,
            savedAt = 0
        )
    }

    private fun dummySitePermissionEntitiesList(): List<SitePermissionsEntity> {
        return listOf(
            SitePermissionsEntity(
                origin = "mozilla.dev",
                localStorage = ALLOWED,
                location = BLOCKED,
                notification = NO_DECISION,
                microphone = ALLOWED,
                camera = BLOCKED,
                bluetooth = ALLOWED,
                savedAt = 0
            ),
            SitePermissionsEntity(
                origin = "mozilla.dev",
                localStorage = ALLOWED,
                location = BLOCKED,
                notification = NO_DECISION,
                microphone = ALLOWED,
                camera = BLOCKED,
                bluetooth = ALLOWED,
                savedAt = 0
            )
        )
    }

    private fun mockDatabase(dao: SitePermissionsDao) = object : SitePermissionsDatabase() {
        override fun sitePermissionsDao() = dao

        override fun createOpenHelper(config: DatabaseConfiguration?): SupportSQLiteOpenHelper = mock()
        override fun createInvalidationTracker(): InvalidationTracker = mock()
        override fun clearAllTables() = Unit
    }
}