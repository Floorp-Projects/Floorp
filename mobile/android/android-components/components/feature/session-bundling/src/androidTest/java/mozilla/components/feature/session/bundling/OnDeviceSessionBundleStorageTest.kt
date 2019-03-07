/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling

import android.content.Context
import android.util.AttributeSet
import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.feature.session.bundling.db.BundleDatabase
import mozilla.components.feature.session.bundling.db.Migrations
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.TimeUnit

private const val MIGRATION_TEST_DB = "migration-test"

class OnDeviceSessionBundleStorageTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: BundleDatabase
    private lateinit var storage: SessionBundleStorage
    private lateinit var engine: Engine

    @get:Rule
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        BundleDatabase::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory()
    )

    @Before
    fun setUp() {
        engine = FakeEngine()

        database = Room.inMemoryDatabaseBuilder(context, BundleDatabase::class.java).build()

        storage = SessionBundleStorage(context, engine, Pair(5L, TimeUnit.MINUTES))
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
        assertNull(storage.restore())

        val firstSnapshot = SessionManager.Snapshot(listOf(
            SessionManager.Snapshot.Item(Session("https://www.mozilla.org")),
            SessionManager.Snapshot.Item(Session("https://www.firefox.com"))
        ), selectedSessionIndex = 1)

        storage.save(firstSnapshot)

        assertNotNull(storage.current())

        val restoredBundle = storage.restore()

        assertNotNull(restoredBundle!!)
        assertEquals(2, restoredBundle.urls.size)
        assertEquals("https://www.mozilla.org", restoredBundle.urls[0])
        assertEquals("https://www.firefox.com", restoredBundle.urls[1])

        val restoredSnapshot = restoredBundle.restoreSnapshot()

        assertNotNull(restoredSnapshot!!)

        assertFalse(restoredSnapshot.isEmpty())
        assertEquals(2, restoredSnapshot.sessions.size)
        assertEquals("https://www.mozilla.org", restoredSnapshot.sessions[0].session.url)
        assertEquals("https://www.firefox.com", restoredSnapshot.sessions[1].session.url)
        assertEquals(1, restoredSnapshot.selectedSessionIndex)
    }

    @Test
    fun migrate1to2() {
        helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            execSQL("INSERT INTO " +
                "bundles " +
                "(id, state, saved_at, urls) " +
                "VALUES " +
                "(1, 'def', 750, 'https://www.mozilla.org')")
        }

        val db = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB, 2, true, Migrations.migration_1_2)

        db.query("SELECT COUNT(*) as count FROM bundles").use { cursor ->
            assertEquals(1, cursor.columnCount)
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(0, cursor.getInt(cursor.getColumnIndexOrThrow("count")))
        }
    }

    private class FakeEngine : Engine {
        override fun createView(context: Context, attrs: AttributeSet?): EngineView {
            throw NotImplementedError()
        }

        override fun createSession(private: Boolean): EngineSession {
            throw NotImplementedError()
        }

        override fun createSessionState(json: JSONObject): EngineSessionState {
            return object : EngineSessionState {
                override fun toJSON(): JSONObject {
                    throw NotImplementedError()
                }
            }
        }

        override fun name(): String {
            return "fake"
        }

        override fun speculativeConnect(url: String) {
            throw NotImplementedError()
        }

        override val settings: Settings
            get() = throw NotImplementedError()
    }
}
