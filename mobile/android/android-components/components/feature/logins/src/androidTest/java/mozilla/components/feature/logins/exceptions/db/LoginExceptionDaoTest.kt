/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins.exceptions.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class LoginExceptionDaoTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    private lateinit var database: LoginExceptionDatabase
    private lateinit var loginExceptionDao: LoginExceptionDao
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        database = Room.inMemoryDatabaseBuilder(context, LoginExceptionDatabase::class.java).build()
        loginExceptionDao = database.loginExceptionDao()
        executor = Executors.newSingleThreadExecutor()
    }

    @After
    fun tearDown() {
        database.close()
        executor.shutdown()
    }

    @Test
    fun testAddingLoginException() {
        val exception = LoginExceptionEntity(
            origin = "mozilla.org",
        ).also {
            it.id = loginExceptionDao.insertLoginException(it)
        }

        val dataSource = loginExceptionDao.getLoginExceptionsPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(1, pagedList.size)
        assertEquals(exception, pagedList[0]!!)
    }

    @Test
    fun testRemovingLoginException() {
        val exception1 = LoginExceptionEntity(
            origin = "mozilla.org",
        ).also {
            it.id = loginExceptionDao.insertLoginException(it)
        }

        val exception2 = LoginExceptionEntity(
            origin = "firefox.com",
        ).also {
            it.id = loginExceptionDao.insertLoginException(it)
        }

        loginExceptionDao.deleteLoginException(exception1)

        val dataSource = loginExceptionDao.getLoginExceptionsPaged().create()

        val pagedList = PagedList.Builder(dataSource, 10)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        assertEquals(1, pagedList.size)
        assertEquals(exception2, pagedList[0])
    }
}
