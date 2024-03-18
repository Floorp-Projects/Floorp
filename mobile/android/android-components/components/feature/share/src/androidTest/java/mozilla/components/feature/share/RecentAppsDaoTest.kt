/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share

import android.content.Context
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import mozilla.components.feature.share.db.RecentAppEntity
import mozilla.components.feature.share.db.RecentAppsDao
import mozilla.components.feature.share.db.RecentAppsDatabase
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class RecentAppsDaoTest {

    private lateinit var database: RecentAppsDatabase
    private lateinit var dao: RecentAppsDao

    @Before
    fun setup() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        database = Room.inMemoryDatabaseBuilder(context, RecentAppsDatabase::class.java).build()
        dao = database.recentAppsDao()
    }

    @After
    fun tearDown() {
        database.close()
    }

    @Test
    fun testGetTwoMostRecentApps() {
        assertEquals(emptyList<RecentApp>(), dao.getRecentAppsUpTo(2))

        dao.insertRecentApps(
            listOf(
                RecentAppEntity("first"),
                RecentAppEntity("second"),
                RecentAppEntity("third"),
            ),
        )

        assertEquals(
            listOf(
                RecentAppEntity("first"),
                RecentAppEntity("second"),
            ),
            dao.getRecentAppsUpTo(2),
        )
    }

    @Test
    fun testIncrementSelectedAppCountAndDecayAllOthers() {
        val activityName = "activityName"
        val selectedAppEntity = RecentAppEntity(score = 1.0, activityName = activityName)
        val otherAppEntity = RecentAppEntity(score = 100.0, activityName = "other")
        val allRecentApps = listOf(selectedAppEntity, otherAppEntity)

        dao.insertRecentApps(allRecentApps)

        dao.updateRecentAppAndDecayRest(activityName)

        val allAppsResult = dao.getRecentAppsUpTo(2)
        val selectedResult = allAppsResult.first { it.activityName == activityName }
        val otherResult = allAppsResult.first { it.activityName == "other" }

        assertEquals(95.0, otherResult.score, 0.0001)
        assertEquals(2.0, selectedResult.score, 0.0001)
    }

    @Test
    fun testAddNewlyInstalledAppsToOurDatabase() {
        val firstActivityName = "first"
        val secondActivityName = "second"
        val thirdActivityName = "third"
        val fourthActivityName = "fourth"
        val appsInDatabase = listOf(
            RecentAppEntity(firstActivityName),
            RecentAppEntity(secondActivityName),
            RecentAppEntity(thirdActivityName),
        )
        val additionalApp = RecentAppEntity(fourthActivityName)

        dao.insertRecentApps(appsInDatabase)
        dao.insertRecentApps(appsInDatabase + additionalApp)

        assertEquals(appsInDatabase + additionalApp, dao.getRecentAppsUpTo(7))
    }

    @Test
    fun testDeleteAnAppFromOurDatabase() {
        val deleteAppName = "delete"
        val recentApp = RecentAppEntity(score = 1.0, activityName = deleteAppName)

        dao.insertRecentApps(listOf(recentApp))

        dao.deleteRecentApp(recentApp.activityName)

        assertEquals(emptyList<RecentApp>(), dao.getRecentAppsUpTo(1))
    }
}
