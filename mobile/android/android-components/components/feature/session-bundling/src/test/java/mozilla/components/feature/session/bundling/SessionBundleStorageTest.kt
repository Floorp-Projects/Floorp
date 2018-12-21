/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling

import android.arch.lifecycle.LiveData
import android.arch.paging.DataSource
import android.arch.persistence.db.SupportSQLiteOpenHelper
import android.arch.persistence.room.DatabaseConfiguration
import android.arch.persistence.room.InvalidationTracker
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.db.BundleDao
import mozilla.components.feature.session.bundling.db.BundleDatabase
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.concurrent.TimeUnit

class SessionBundleStorageTest {
    @Test
    fun `restore loads last bundle using lifetime`() {
        val dao: BundleDao = mock()

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
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
            override fun getBundles(limit: Int): LiveData<List<BundleEntity>> = TODO()
            override fun getBundlesPaged(): DataSource.Factory<Int, BundleEntity> = TODO()

            override fun getLastBundle(since: Long): BundleEntity? = bundle
        }

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        assertNull(storage.current())
        assertEquals(bundle, storage.restore())
        assertEquals(bundle, storage.current())
    }

    @Test
    fun `save will create new bundle`() {
        val dao: BundleDao = mock()

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
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

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
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
        val bundle: BundleEntity = mock()

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        storage.restore()

        assertNotNull(storage.current())

        storage.remove(bundle)

        assertNull(storage.current())

        verify(dao).deleteBundle(bundle)
    }

    @Test
    fun `removeAll will remove all bundles`() {
        val bundle: BundleEntity = mock()

        val dao: BundleDao = mock()
        doReturn(bundle).`when`(dao).getLastBundle(ArgumentMatchers.anyLong())

        val database = mockDatabase(dao)

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
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

        val storage = spy(SessionBundleStorage(mock(), Pair(2, TimeUnit.HOURS)).apply {
            databaseInitializer = { database }
        })

        storage.restore()

        assertEquals(bundle, storage.current())

        val newBundle: BundleEntity = mock()

        storage.use(newBundle)

        assertEquals(newBundle, storage.current())

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        storage.save(snapshot)

        verify(newBundle).updateFrom(snapshot)
        verify(dao).updateBundle(newBundle)
    }

    private fun mockDatabase(bundleDao: BundleDao) = object : BundleDatabase() {
        override fun bundleDao() = bundleDao

        override fun createOpenHelper(config: DatabaseConfiguration?): SupportSQLiteOpenHelper = mock()
        override fun createInvalidationTracker(): InvalidationTracker = mock()
        override fun clearAllTables() = Unit
    }
}
