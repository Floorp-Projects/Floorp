/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import androidx.room.DatabaseConfiguration
import androidx.room.InvalidationTracker
import androidx.sqlite.db.SupportSQLiteOpenHelper
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.update.AddonUpdater.Status.SuccessfullyUpdated
import mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateAttemptStorage
import mozilla.components.feature.addons.update.db.UpdateAttemptDao
import mozilla.components.feature.addons.update.db.UpdateAttemptsDatabase
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.util.Date

@RunWith(AndroidJUnit4::class)
class UpdateAttemptStorageTest {

    private lateinit var mockDAO: UpdateAttemptDao
    private lateinit var storage: UpdateAttemptStorage

    @Before
    fun setup() {
        mockDAO = mock()
        storage = spy(
            UpdateAttemptStorage(mock()).apply {
                databaseInitializer = { mockDatabase(mockDAO) }
            },
        )
    }

    @Test
    fun `save or update a request`() {
        storage.saveOrUpdate(createNewRequest())

        verify(mockDAO).insertOrUpdate(any())
    }

    @Test
    fun `find a request by addonId`() {
        storage.findUpdateAttemptBy(addonId = "addonId")

        verify(mockDAO).getUpdateAttemptFor("addonId")
    }

    @Test
    fun `remove a request`() {
        storage.remove("addonId")

        verify(mockDAO).deleteUpdateAttempt(any())
    }

    private fun createNewRequest(): AddonUpdater.UpdateAttempt {
        return AddonUpdater.UpdateAttempt(
            addonId = "mozilla-dev-ext",
            date = Date(),
            status = SuccessfullyUpdated,
        )
    }

    private fun mockDatabase(dao: UpdateAttemptDao) = object : UpdateAttemptsDatabase() {
        override fun updateAttemptDao() = dao
        override fun createOpenHelper(config: DatabaseConfiguration?): SupportSQLiteOpenHelper = mock()
        override fun createInvalidationTracker(): InvalidationTracker = mock()
        override fun clearAllTables() = Unit
    }
}
