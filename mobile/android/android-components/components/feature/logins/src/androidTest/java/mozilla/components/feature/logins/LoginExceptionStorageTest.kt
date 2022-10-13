/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.logins.exceptions.LoginException
import mozilla.components.feature.logins.exceptions.LoginExceptionStorage
import mozilla.components.feature.logins.exceptions.db.LoginExceptionDatabase
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@Suppress("LargeClass")
class LoginExceptionStorageTest {
    private lateinit var context: Context
    private lateinit var storage: LoginExceptionStorage
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @get:Rule
    @Suppress("DEPRECATION")
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        LoginExceptionStorage::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory(),
    )

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        val database =
            Room.inMemoryDatabaseBuilder(context, LoginExceptionDatabase::class.java).build()

        storage =
            LoginExceptionStorage(
                context,
            )
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        executor.shutdown()
    }

    @Test
    fun testAddingExceptions() {
        storage.addLoginException("mozilla.org")
        storage.addLoginException("firefox.com")

        val exceptions = getAllExceptions()

        assertEquals(2, exceptions.size)

        assertEquals("mozilla.org", exceptions[0].origin)
        assertEquals("firefox.com", exceptions[1].origin)
    }

    @Test
    fun testRemovingExceptions() {
        storage.addLoginException("mozilla.org")
        storage.addLoginException("firefox.com")

        getAllExceptions().let { exceptions ->
            assertEquals(2, exceptions.size)
            storage.removeLoginException(exceptions[0])
        }

        getAllExceptions().let { exceptions ->
            assertEquals(1, exceptions.size)
            assertEquals("firefox.com", exceptions[0].origin)
        }
    }

    @Test
    fun testGettingExceptions() = runBlocking {
        storage.addLoginException("mozilla.org")
        storage.addLoginException("firefox.com")

        val exceptions = storage.getLoginExceptions().first()

        assertNotNull(exceptions)
        assertEquals(2, exceptions.size)

        with(exceptions[0]) {
            assertEquals("mozilla.org", origin)
        }

        with(exceptions[1]) {
            assertEquals("firefox.com", origin)
        }
    }

    @Test
    fun testGettingExceptionsByOrigin() = runBlocking {
        storage.addLoginException("mozilla.org")
        storage.addLoginException("firefox.com")

        val exception = storage.findExceptionByOrigin("mozilla.org")

        assertNotNull(exception)
        assertEquals("mozilla.org", exception!!.origin)
    }

    @Test
    fun testGettingNoExceptionsByOrigin() = runBlocking {
        storage.addLoginException("mozilla.org")
        storage.addLoginException("firefox.com")

        val exception = storage.findExceptionByOrigin("testsite.org")

        assertNull(exception)
    }

    private fun getAllExceptions(): List<LoginException> {
        val dataSource = storage.getLoginExceptionsPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        return pagedList.toList()
    }
}
