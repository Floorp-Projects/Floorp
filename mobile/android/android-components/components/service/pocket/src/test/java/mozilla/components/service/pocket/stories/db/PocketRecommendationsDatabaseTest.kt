/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.spocs.db.SpocEntity
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

private const val MIGRATION_TEST_DB = "migration-test"

@RunWith(AndroidJUnit4::class)
class PocketRecommendationsDatabaseTest {
    @get:Rule
    @Suppress("DEPRECATION")
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        PocketRecommendationsDatabase::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory()
    )

    @OptIn(ExperimentalCoroutinesApi::class)
    @Test
    fun `GIVEN database at version 1 WHEN needing to migrate to version 2 THEN add a new spocs table`() = runTest {
        val story = PocketTestResources.dbExpectedPocketStory
        // Create the database with the version 1 schema
        val dbVersion1 = helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_STORIES}' " +
                    "(url, title, imageUrl, publisher, category, timeToRead, timesShown) " +
                    "VALUES (" +
                    "'${story.url}'," +
                    "'${story.title}'," +
                    "'${story.imageUrl}'," +
                    "'${story.publisher}'," +
                    "'${story.category}'," +
                    "'${story.timeToRead}'," +
                    "'${story.timesShown}'" +
                    ")"
            )
        }
        // Validate the persisted data which will be re-checked after migration
        dbVersion1.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}"
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(
                story,
                PocketStoryEntity(
                    url = cursor.getString(0),
                    title = cursor.getString(1),
                    imageUrl = cursor.getString(2),
                    publisher = cursor.getString(3),
                    category = cursor.getString(4),
                    timeToRead = cursor.getInt(5),
                    timesShown = cursor.getLong(6),
                )
            )
        }

        val spoc = PocketTestResources.dbExpectedPocketSpoc
        // Migrate the initial database to the version 2 schema
        val dbVersion2 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB, 2, true, Migrations.migration_1_2
        ).apply {
            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}' " +
                    "(url, title, imageUrl, sponsor, clickShim, impressionShim) " +
                    "VALUES (" +
                    "'${spoc.url}'," +
                    "'${spoc.title}'," +
                    "'${spoc.imageUrl}'," +
                    "'${spoc.sponsor}'," +
                    "'${spoc.clickShim}'," +
                    "'${spoc.impressionShim}'" +
                    ")"
            )
        }
        // Re-check the initial data we had
        dbVersion2.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}"
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(
                story,
                PocketStoryEntity(
                    url = cursor.getString(0),
                    title = cursor.getString(1),
                    imageUrl = cursor.getString(2),
                    publisher = cursor.getString(3),
                    category = cursor.getString(4),
                    timeToRead = cursor.getInt(5),
                    timesShown = cursor.getLong(6),
                )
            )
        }
        // Finally validate that the new spocs are persisted successfully
        dbVersion2.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}"
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(
                spoc,
                SpocEntity(
                    url = cursor.getString(0),
                    title = cursor.getString(1),
                    imageUrl = cursor.getString(2),
                    sponsor = cursor.getString(3),
                    clickShim = cursor.getString(4),
                    impressionShim = cursor.getString(5),
                )
            )
        }
    }
}
