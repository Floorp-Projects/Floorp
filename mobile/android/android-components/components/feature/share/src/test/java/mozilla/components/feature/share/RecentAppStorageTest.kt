/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share

import android.content.Context
import mozilla.components.feature.share.db.RecentAppEntity
import mozilla.components.feature.share.db.RecentAppsDao
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.verify

class RecentAppStorageTest {

    private lateinit var context: Context
    private lateinit var recentAppsDao: RecentAppsDao
    private lateinit var recentAppsStorage: RecentAppsStorage

    @Before
    fun setup() {
        context = mock()
        recentAppsDao = mock()

        recentAppsStorage = RecentAppsStorage(context)
        recentAppsStorage.recentAppsDao = lazyOf(recentAppsDao)
    }

    @Test
    fun `get the two most recent apps`() {
        whenever(recentAppsDao.getRecentAppsUpTo(2)).thenReturn(emptyList())

        assertEquals(emptyList<RecentApp>(), recentAppsStorage.getRecentAppsUpTo(2))
    }

    @Test
    fun `increment selected app count and decay all others`() {
        val activityName = "activityName"

        recentAppsStorage.updateRecentApp(activityName)

        verify(recentAppsDao).updateRecentAppAndDecayRest(activityName)
    }

    @Test
    fun `add newly installed apps to our database`() {
        val firstActivityName = "first"
        val secondActivityName = "second"
        val thirdActivityName = "third"
        val fourthActivityName = "fourth"
        val currentApps = listOf(firstActivityName, secondActivityName, thirdActivityName, fourthActivityName)
        val appsInDatabase = listOf(
            RecentAppEntity(firstActivityName),
            RecentAppEntity(secondActivityName),
            RecentAppEntity(thirdActivityName),
            RecentAppEntity(fourthActivityName),
        )

        recentAppsStorage.updateDatabaseWithNewApps(currentApps)

        verify(recentAppsDao).insertRecentApps(appsInDatabase)
    }

    @Test
    fun `delete an app from our database`() {
        val deleteAppName = "delete"
        val recentApp = RecentAppEntity(score = 1.0, activityName = deleteAppName)

        recentAppsStorage.deleteRecentApp(recentApp.activityName)

        verify(recentAppsDao).deleteRecentApp(deleteAppName)
    }
}
