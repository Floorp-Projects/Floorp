/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import android.content.Context
import androidx.core.net.toUri
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status.ALLOWED
import mozilla.components.concept.engine.permission.SitePermissions.Status.BLOCKED
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class SitePermissionsDaoTest {

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: SitePermissionsDatabase
    private lateinit var dao: SitePermissionsDao

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, SitePermissionsDatabase::class.java).build()
        dao = database.sitePermissionsDao()
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun testInsertingAndReadingSitePermissions() {
        val origin = insertMockSitePermissions("https://www.mozilla.org")

        val siteFromDb = dao.getSitePermissionsBy(origin)!!.toSitePermission()

        assertEquals(origin, siteFromDb.origin)
        assertEquals(BLOCKED, siteFromDb.camera)
        assertEquals(AutoplayStatus.BLOCKED, siteFromDb.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, siteFromDb.autoplayInaudible)
    }

    @Test
    fun testRemoveAllSitePermissions() {
        for (index in 1..4) {
            val origin = insertMockSitePermissions("https://www.mozilla$index.org")

            val sitePermissionFromDb = dao.getSitePermissionsBy(origin)

            assertEquals(origin, sitePermissionFromDb!!.origin)
        }

        dao.deleteAllSitePermissions()

        val isEmpty = dao.getSitePermissions().isEmpty()

        assertTrue(isEmpty)
    }

    @Test
    fun testUpdateAndDeleteSitePermissions() {
        val origin = insertMockSitePermissions("https://www.mozilla.org")
        var siteFromDb = dao.getSitePermissionsBy(origin)!!.toSitePermission()

        assertEquals(BLOCKED, siteFromDb.camera)
        assertEquals(AutoplayStatus.BLOCKED, siteFromDb.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, siteFromDb.autoplayInaudible)

        dao.update(
            siteFromDb.copy(
                camera = ALLOWED,
                autoplayInaudible = AutoplayStatus.ALLOWED,
                autoplayAudible = AutoplayStatus.ALLOWED,
            ).toSitePermissionsEntity(),
        )

        siteFromDb = dao.getSitePermissionsBy(origin)!!.toSitePermission()

        assertEquals(ALLOWED, siteFromDb.camera)
        assertEquals(AutoplayStatus.ALLOWED, siteFromDb.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, siteFromDb.autoplayInaudible)

        dao.deleteSitePermissions(siteFromDb.toSitePermissionsEntity())

        val notFoundSitePermissions = dao.getSitePermissionsBy(origin)?.toSitePermission()

        assertNull(notFoundSitePermissions)
    }

    private fun insertMockSitePermissions(url: String): String {
        val origin = url.toUri().host!!
        val sitePermissions = SitePermissions(
            origin = origin,
            camera = BLOCKED,
            savedAt = System.currentTimeMillis(),
        )
        dao.insert(
            sitePermissions.toSitePermissionsEntity(),
        )
        return origin
    }
}
