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
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.concept.engine.permission.SitePermissions.Status
import mozilla.components.feature.sitepermissions.OnDiskSitePermissionsStorage
import mozilla.components.support.ktx.kotlin.getOrigin
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test

private const val MIGRATION_TEST_DB = "migration-test"

class OnDeviceSitePermissionsStorageTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var storage: OnDiskSitePermissionsStorage
    private lateinit var database: SitePermissionsDatabase

    @get:Rule
    @Suppress("DEPRECATION")
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        SitePermissionsDatabase::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory(),
    )

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, SitePermissionsDatabase::class.java).build()
        storage = OnDiskSitePermissionsStorage(context)
        storage.databaseInitializer = {
            database
        }
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun testStorageInteraction() = runTest {
        val origin = "https://www.mozilla.org".toUri().host!!
        val sitePermissions = SitePermissions(
            origin = origin,
            camera = Status.BLOCKED,
            savedAt = System.currentTimeMillis(),
        )
        storage.save(sitePermissions)
        val sitePermissionsFromStorage = storage.findSitePermissionsBy(origin)!!

        assertEquals(origin, sitePermissionsFromStorage.origin)
        assertEquals(Status.BLOCKED, sitePermissionsFromStorage.camera)
    }

    @Test
    fun migrate1to2() {
        val dbVersion1 = helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera_front,camera_back,bluetooth,local_storage,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,1,1)",
            )
        }

        dbVersion1.query("SELECT * FROM site_permissions").use { cursor ->
            assertEquals(9, cursor.columnCount)
        }

        val dbVersion2 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB,
            2,
            true,
            Migrations.migration_1_2,
        ).apply {
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,1)",
            )
        }

        dbVersion2.query("SELECT * FROM site_permissions").use { cursor ->
            assertEquals(8, cursor.columnCount)

            cursor.moveToFirst()
            assertEquals(1, cursor.getInt(cursor.getColumnIndexOrThrow("camera")))
        }
    }

    @Test
    fun migrate2to3() {
        helper.createDatabase(MIGRATION_TEST_DB, 2).apply {
            query("SELECT * FROM site_permissions").use { cursor ->
                assertEquals(8, cursor.columnCount)
            }
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,1)",
            )
        }

        val dbVersion3 = helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 3, true, Migrations.migration_2_3)

        dbVersion3.query("SELECT * FROM site_permissions").use { cursor ->
            assertEquals(10, cursor.columnCount)

            cursor.moveToFirst()
            assertEquals(Status.BLOCKED.id, cursor.getInt(cursor.getColumnIndexOrThrow("autoplay_audible")))
            assertEquals(Status.ALLOWED.id, cursor.getInt(cursor.getColumnIndexOrThrow("autoplay_inaudible")))
        }
    }

    @Test
    fun migrate3to4() {
        helper.createDatabase(MIGRATION_TEST_DB, 3).apply {
            query("SELECT * FROM site_permissions").use { cursor ->
                assertEquals(10, cursor.columnCount)
            }
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,autoplay_audible,autoplay_inaudible,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,1,1,1)",
            )
        }

        val dbVersion3 = helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 4, true, Migrations.migration_3_4)

        dbVersion3.query("SELECT * FROM site_permissions").use { cursor ->
            assertEquals(11, cursor.columnCount)

            cursor.moveToFirst()
            assertEquals(Status.NO_DECISION.id, cursor.getInt(cursor.getColumnIndexOrThrow("media_key_system_access")))
        }
    }

    @Test
    fun migrate4to5() {
        helper.createDatabase(MIGRATION_TEST_DB, 4).apply {
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,autoplay_audible,autoplay_inaudible,media_key_system_access,saved_at) " +
                    "VALUES " +
                    "('mozilla.org',1,1,1,1,1,1,0,0,1,1)",
            )
        }

        val dbVersion5 = helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 5, true, Migrations.migration_4_5)

        dbVersion5.query("SELECT * FROM site_permissions").use { cursor ->
            cursor.moveToFirst()
            assertEquals(AutoplayStatus.BLOCKED.id, cursor.getInt(cursor.getColumnIndexOrThrow("autoplay_audible")))
            assertEquals(AutoplayStatus.ALLOWED.id, cursor.getInt(cursor.getColumnIndexOrThrow("autoplay_inaudible")))
        }
    }

    @Test
    fun migrate5to6() {
        val url = "https://permission.site/"

        helper.createDatabase(MIGRATION_TEST_DB, 5).apply {
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,autoplay_audible,autoplay_inaudible,media_key_system_access,saved_at) " +
                    "VALUES " +
                    "('${url.tryGetHostFromUrl()}',1,1,1,1,1,1,0,0,1,1)",
            )
        }

        val dbVersion6 =
            helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 6, true, Migrations.migration_5_6)

        dbVersion6.query("SELECT * FROM site_permissions").use { cursor ->
            cursor.moveToFirst()
            val urlDB = cursor.getString(cursor.getColumnIndexOrThrow("origin"))
            assertEquals(url.getOrigin(), urlDB)
        }
    }

    @Test
    fun migrate6to7() {
        val url = "https://permission.site/"

        helper.createDatabase(MIGRATION_TEST_DB, 6).apply {
            execSQL(
                "INSERT INTO " +
                    "site_permissions " +
                    "(origin, location, notification, microphone,camera,bluetooth,local_storage,autoplay_audible,autoplay_inaudible,media_key_system_access,saved_at) " +
                    "VALUES " +
                    "('${url.tryGetHostFromUrl()}',1,1,1,1,1,1,-1,-1,1,1)",
            ) // Block audio and video.
        }

        val dbVersion6 =
            helper.runMigrationsAndValidate(MIGRATION_TEST_DB, 7, true, Migrations.migration_6_7)

        dbVersion6.query("SELECT * FROM site_permissions").use { cursor ->
            cursor.moveToFirst()
            val audible = cursor.getInt(cursor.getColumnIndexOrThrow("autoplay_audible"))
            val inaudible = cursor.getInt(cursor.getColumnIndexOrThrow("autoplay_inaudible"))
            assertEquals(-1, audible) // Block audio.
            assertEquals(1, inaudible) // Allow inaudible.
        }
    }
}
