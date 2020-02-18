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
        val packageName = "packageName"
        val selectedAppEntity =
                RecentAppEntity(score = 1.0, packageName = packageName)
        val otherAppEntity =
                RecentAppEntity(score = 100.0, packageName = "other")
        val allRecentApps = ArrayList<RecentAppEntity>()
        allRecentApps.add(selectedAppEntity)
        allRecentApps.add(otherAppEntity)

        whenever(recentAppsDao.getRecentApp(packageName)).thenReturn(selectedAppEntity)
        whenever(recentAppsDao.getAllRecentApps()).thenReturn(allRecentApps)

        recentAppsStorage.updateRecentApp(packageName)

        verify(recentAppsDao).updateRecentApp(selectedAppEntity)
        assertEquals(95.0, otherAppEntity.score, 0.0001)
        assertEquals(2.0, selectedAppEntity.score, 0.0001)
        verify(recentAppsDao).updateAllRecentApp(allRecentApps)
    }

    @Test
    fun `add newly installed apps to our database`() {
        val appsInDatabase = ArrayList<RecentAppEntity>()
        val firstPackageName = "first"
        val secondPackageName = "second"
        val thirdPackageName = "third"
        val fourthPackageName = "fourth"
        appsInDatabase.add(
                RecentAppEntity(
                        score = 100.0,
                        packageName = firstPackageName
                )
        )
        appsInDatabase.add(
                RecentAppEntity(
                        score = 20.0,
                        packageName = secondPackageName
                )
        )
        appsInDatabase.add(
                RecentAppEntity(
                        score = 50.0,
                        packageName = thirdPackageName
                )
        )
        val result = RecentAppEntity(
                packageName = fourthPackageName,
                score = RecentAppsStorage.DEFAULT_COUNT
        )

        val currentApps = listOf(firstPackageName, secondPackageName, thirdPackageName, fourthPackageName)

        whenever(recentAppsDao.getRecentApp(fourthPackageName)).thenReturn(null)

        recentAppsStorage.updateDatabaseWithNewApps(currentApps)

        verify(recentAppsDao).insertRecentApp(result)
    }
}
