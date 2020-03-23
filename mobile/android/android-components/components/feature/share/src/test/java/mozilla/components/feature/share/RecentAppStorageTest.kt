/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share

import android.content.Context
import mozilla.components.feature.share.db.RecentAppEntity
import mozilla.components.feature.share.db.RecentAppsDao
import mozilla.components.feature.share.db.RecentAppsDatabase
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify

class RecentAppStorageTest {

    private lateinit var recentAppsDatabase: RecentAppsDatabase
    private lateinit var context: Context
    private lateinit var recentAppsDao: RecentAppsDao

    private lateinit var recentAppsStorage: RecentAppsStorage

    @Before
    fun setup() {
        recentAppsDatabase = mock()
        context = mock()
        recentAppsDao = mock()
        whenever(recentAppsDatabase.recentAppsDao()).thenReturn(recentAppsDao)

        recentAppsStorage = RecentAppsStorage(context)
        recentAppsStorage.database = lazyOf(recentAppsDatabase)
    }

    @Test
    fun `get the two most recent apps`() {
        val result = ArrayList<RecentAppEntity>()
        whenever(recentAppsDao.getRecentAppsUpTo(2)).thenReturn(result)

        assertEquals(result, recentAppsStorage.getRecentAppsUpTo(2))
    }

    @Test
    fun `increment selected app count and decay all others`() {
        val activityName = "activityName"
        val selectedAppEntity =
                RecentAppEntity(score = 1.0, activityName = activityName)
        val otherAppEntity =
                RecentAppEntity(score = 100.0, activityName = "other")
        val allRecentApps = ArrayList<RecentAppEntity>()
        allRecentApps.add(selectedAppEntity)
        allRecentApps.add(otherAppEntity)

        whenever(recentAppsDao.getRecentApp(activityName)).thenReturn(selectedAppEntity)
        whenever(recentAppsDao.getAllRecentApps()).thenReturn(allRecentApps)

        recentAppsStorage.updateRecentApp(activityName)

        verify(recentAppsDao).updateRecentApp(selectedAppEntity)
        assertEquals(95.0, otherAppEntity.score, 0.0001)
        assertEquals(2.0, selectedAppEntity.score, 0.0001)
        verify(recentAppsDao).updateAllRecentApp(allRecentApps)
    }

    @Test
    fun `add newly installed apps to our database`() {
        val appsInDatabase = ArrayList<RecentAppEntity>()
        val firstActivityName = "first"
        val secondActivityName = "second"
        val thirdActivityName = "third"
        val fourthActivityName = "fourth"
        appsInDatabase.add(
                RecentAppEntity(
                        score = 100.0,
                        activityName = firstActivityName
                )
        )
        appsInDatabase.add(
                RecentAppEntity(
                        score = 20.0,
                        activityName = secondActivityName
                )
        )
        appsInDatabase.add(
                RecentAppEntity(
                        score = 50.0,
                        activityName = thirdActivityName
                )
        )
        val result = RecentAppEntity(
                activityName = fourthActivityName,
                score = RecentAppsStorage.DEFAULT_COUNT
        )

        val currentApps = listOf(firstActivityName, secondActivityName, thirdActivityName, fourthActivityName)

        whenever(recentAppsDao.getRecentApp(fourthActivityName)).thenReturn(null)

        recentAppsStorage.updateDatabaseWithNewApps(currentApps)

        verify(recentAppsDao).insertRecentApp(result)
    }
}
