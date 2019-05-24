/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.feature.session.bundling

import androidx.lifecycle.LiveData
import android.content.Context
import android.util.AtomicFile
import android.util.AttributeSet
import androidx.paging.DataSource
import androidx.room.DatabaseConfiguration
import androidx.room.InvalidationTracker
import androidx.sqlite.db.SupportSQLiteOpenHelper
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.feature.session.bundling.adapter.SessionBundleAdapter
import mozilla.components.feature.session.bundling.db.BundleDao
import mozilla.components.feature.session.bundling.db.BundleDatabase
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner
import java.io.File
import java.util.UUID
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class SessionBundleStorageTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var engine: Engine

    @Before
    fun setUp() {
        engine = FakeEngine()
    }

    @Test
    fun `restore loads last bundle using lifetime`() {
        val dao: BundleDao = mock()

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { mockDatabase(dao) }
        })
        doReturn(1234567890L).`when`(storage).now()

        val bundle = storage.restore()

        assertNull(bundle)
        verify(dao).getLastBundle(1227367890L) // since 2 hours ago
    }

    @Test
    fun `current returns restored bundle`() {
        val bundle: BundleEntity = mock()

        val dao = object : BundleDao {
            override fun insertBundle(bundle: BundleEntity): Long = TODO()
            override fun updateBundle(bundle: BundleEntity) = TODO()
            override fun deleteBundle(bundle: BundleEntity) = TODO()
            override fun getBundles(since: Long, limit: Int): LiveData<List<BundleEntity>> = TODO()
            override fun getBundlesPaged(since: Long): DataSource.Factory<Int, BundleEntity> = TODO()

            override fun getLastBundle(since: Long): BundleEntity? = bundle
        }

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        assertNull(storage.current())

        assertEquals(bundle, (storage.restore() as SessionBundleAdapter).actual)
        assertEquals(bundle, (storage.current() as SessionBundleAdapter).actual)
    }

    @Test
    fun `save will create new bundle`() {
        val dao: BundleDao = mock()

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { mockDatabase(dao) }
        })

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        assertNull(storage.current())

        storage.save(snapshot)

        verify(dao).insertBundle(any())

        assertNotNull(storage.current())
    }

    @Test
    fun `save will update existing bundle`() {
        val bundle: BundleEntity = mock()
        `when`(bundle.stateFile(any(), any())).thenReturn(mockAtomicFile())

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        storage.restore()

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        storage.save(snapshot)

        verify(bundle).updateFrom(snapshot)
        verify(dao).updateBundle(bundle)
    }

    @Test
    fun `remove will remove existing bundle`() {
        val atomicFile = mockAtomicFile()

        val bundle: BundleEntity = mock()
        `when`(bundle.stateFile(any(), any())).thenReturn(atomicFile)

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        storage.restore()

        assertNotNull(storage.current())

        storage.remove(SessionBundleAdapter(context, engine, bundle))

        assertNull(storage.current())

        verify(dao).deleteBundle(bundle)
        verify(atomicFile).delete()
    }

    @Test
    fun `removeAll will remove all bundles`() {
        val bundle: BundleEntity = mock()

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        storage.restore()

        assertNotNull(storage.current())

        storage.removeAll()

        assertNull(storage.current())
        verify(storage).removeAll()
    }

    @Test
    fun `Use will override current bundle`() {
        val bundle: BundleEntity = mock()

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        storage.restore()

        val current = storage.current()
        assertTrue(current is SessionBundleAdapter)
        assertEquals(bundle, (current as SessionBundleAdapter).actual)

        val newBundle: BundleEntity = mock()
        `when`(newBundle.stateFile(any(), any())).thenReturn(mockAtomicFile())

        storage.use(SessionBundleAdapter(context, engine, newBundle))

        val updated = storage.current()
        assertTrue(updated is SessionBundleAdapter)
        assertEquals(newBundle, (updated as SessionBundleAdapter).actual)

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        storage.save(snapshot)

        verify(newBundle).updateFrom(snapshot)
        verify(dao).updateBundle(newBundle)
    }

    @Test
    fun `An empty snapshot will not be saved as bundle in the database`() {
        val bundle: BundleEntity = mock()

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        }

        val snapshot = SessionManager.Snapshot(emptyList(), -1)
        storage.save(snapshot)

        verifyNoMoreInteractions(dao)
    }

    @Test
    fun `An existing bundle will be removed instead of updated with an empty snapshot`() {
        val file = mockAtomicFile()

        val bundle: BundleEntity = mock()
        `when`(bundle.stateFile(any(), any())).thenReturn(file)

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        }

        storage.restore()

        verify(dao).getLastBundle(ArgumentMatchers.anyLong())

        assertNotNull(storage.current())

        val snapshot = SessionManager.Snapshot(emptyList(), -1)
        storage.save(snapshot)

        verify(bundle, never()).updateFrom(snapshot)
        verify(dao).deleteBundle(bundle)
        verifyNoMoreInteractions(dao)
        verify(file).delete()
    }

    @Test
    fun `State is saved as file and removed with bundle`() {
        val dao: BundleDao = mock()
        val database = mockDatabase(dao)

        val storage = SessionBundleStorage(context, engine, Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        }

        val snapshot = SessionManager.Snapshot(listOf(
            SessionManager.Snapshot.Item(Session("https://www.mozilla.org")),
            SessionManager.Snapshot.Item(Session("https://www.firefox.com"))),
            selectedSessionIndex = 1
        )

        storage.save(snapshot)

        val current = storage.current()
        assertNotNull(current!!)

        assertTrue(current is SessionBundleAdapter)
        val adapter = current as SessionBundleAdapter

        val path = adapter.actual.statePath(context, engine)
        assertTrue(path.exists())
        assertTrue(path.length() > 0)

        storage.remove(current)

        assertFalse(path.exists())
    }

    private fun mockDatabase(bundleDao: BundleDao) = object : BundleDatabase() {
        override fun bundleDao() = bundleDao

        override fun createOpenHelper(config: DatabaseConfiguration?): SupportSQLiteOpenHelper = mock()
        override fun createInvalidationTracker(): InvalidationTracker = mock()
        override fun clearAllTables() = Unit
    }

    private fun mockAtomicFile(): AtomicFile {
        return spy(AtomicFile(File.createTempFile(
            UUID.randomUUID().toString(),
            UUID.randomUUID().toString()
        )))
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
