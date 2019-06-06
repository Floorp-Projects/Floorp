/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import android.content.Context
import androidx.core.net.toUri
import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsStorage
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test

private const val MIGRATION_TEST_DB = "migration-test"

class OnDeviceSitePermissionsStorageTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var storage: SitePermissionsStorage
    private lateinit var database: SitePermissionsDatabase

    @get:Rule
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        SitePermissionsDatabase::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory()
    )

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, SitePermissionsDatabase::class.java).build()
        storage = SitePermissionsStorage(context)
        storage.databaseInitializer = {
            database
        }
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun testStorageInteraction() {
        val origin = "https://www.mozilla.org".toUri().host!!
        val sitePermissions = SitePermissions(
            origin = origin,
            camera = SitePermissions.Status.BLOCKED,
            savedAt = System.currentTimeMillis()
        )
        storage.save(sitePermissions)
        val sitePermissionsFromStorage = storage.findSitePermissionsBy(origin)!!

        assertEquals(origin, sitePermissionsFromStorage.origin)
        assertEquals(SitePermissions.Status.BLOCKED, sitePermissionsFromStorage.camera)
    }

    @Test
    fun migrate1to2() {
        val dbVersion1 = helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera_front,camera_back,bluetooth,local_storage,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,1,1)"
            )
        }

        dbVersion1.query("SELECT * FROM site_permissions").use { cursor ->
            assertEquals(9, cursor.columnCount)
        }

        val dbVersion2 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB, 2, true, Migrations.migration_1_2
        ).apply {

            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,1)"
            )
        }

        dbVersion2.query("SELECT * FROM site_permissions").use { cursor ->
            assertEquals(8, cursor.columnCount)

            cursor.moveToFirst()
            assertEquals(1, cursor.getInt(cursor.getColumnIndexOrThrow("camera")))
        }
    }
}
