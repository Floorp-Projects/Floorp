/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.room.Room
import androidx.room.testing.MigrationTestHelper
import androidx.sqlite.db.framework.FrameworkSQLiteOpenHelperFactory
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.runBlocking
import mozilla.components.service.pocket.spocs.db.SpocEntity
import mozilla.components.service.pocket.spocs.db.SpocImpressionEntity
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase.Companion
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

private const val MIGRATION_TEST_DB = "migration-test"

class PocketRecommendationsDatabaseTest {
    private lateinit var context: Context
    private lateinit var executor: ExecutorService
    private lateinit var database: PocketRecommendationsDatabase

    @get:Rule
    @Suppress("DEPRECATION")
    val helper: MigrationTestHelper = MigrationTestHelper(
        InstrumentationRegistry.getInstrumentation(),
        PocketRecommendationsDatabase::class.java.canonicalName,
        FrameworkSQLiteOpenHelperFactory(),
    )

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        database = Room.inMemoryDatabaseBuilder(context, PocketRecommendationsDatabase::class.java).build()
    }

    @After
    fun tearDown() {
        executor.shutdown()
        database.clearAllTables()
    }

    @Test
    fun `test1To2MigrationAddsNewSpocsTable`() = runBlocking {
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
                    ")",
            )
        }
        // Validate the persisted data which will be re-checked after migration
        dbVersion1.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}",
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
                ),
            )
        }

        // Migrate the initial database to the version 2 schema
        val dbVersion2 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB,
            2,
            true,
            Migrations.migration_1_2,
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
                    ")",
            )
        }
        // Re-check the initial data we had
        dbVersion2.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}",
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
                ),
            )
        }
        // Finally validate that the new spocs are persisted successfully
        dbVersion2.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}",
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(spoc.url, cursor.getString(0))
            assertEquals(spoc.title, cursor.getString(1))
            assertEquals(spoc.imageUrl, cursor.getString(2))
            assertEquals(spoc.sponsor, cursor.getString(3))
            assertEquals(spoc.clickShim, cursor.getString(4))
            assertEquals(spoc.impressionShim, cursor.getString(5))
        }
    }

    @Test
    fun `test2To3MigrationDropsOldSpocsTableAndAddsNewSpocsAndSpocsImpressionsTables`() = runBlocking {
        // Create the database with the version 2 schema
        val dbVersion2 = helper.createDatabase(MIGRATION_TEST_DB, 2).apply {
            execSQL(
                "INSERT INTO " +
                    "'${Companion.TABLE_NAME_STORIES}' " +
                    "(url, title, imageUrl, publisher, category, timeToRead, timesShown) " +
                    "VALUES (" +
                    "'${story.url}'," +
                    "'${story.title}'," +
                    "'${story.imageUrl}'," +
                    "'${story.publisher}'," +
                    "'${story.category}'," +
                    "'${story.timeToRead}'," +
                    "'${story.timesShown}'" +
                    ")",
            )
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
                    ")",
            )
        }

        // Validate the recommended stories data which will be re-checked after migration
        dbVersion2.query(
            "SELECT * FROM ${Companion.TABLE_NAME_STORIES}",
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
                ),
            )
        }

        // Migrate to v3 database
        val dbVersion3 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB,
            3,
            true,
            Migrations.migration_2_3,
        )

        // Check that recommended stories are unchanged.
        dbVersion3.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}",
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
                ),
            )
        }

        // Finally validate that we have two new empty tables for spocs and spocs impressions.
        dbVersion3.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}",
        ).use { cursor ->
            assertEquals(0, cursor.count)
            assertEquals(11, cursor.columnCount)

            assertEquals("id", cursor.getColumnName(0))
            assertEquals("url", cursor.getColumnName(1))
            assertEquals("title", cursor.getColumnName(2))
            assertEquals("imageUrl", cursor.getColumnName(3))
            assertEquals("sponsor", cursor.getColumnName(4))
            assertEquals("clickShim", cursor.getColumnName(5))
            assertEquals("impressionShim", cursor.getColumnName(6))
            assertEquals("priority", cursor.getColumnName(7))
            assertEquals("lifetimeCapCount", cursor.getColumnName(8))
            assertEquals("flightCapCount", cursor.getColumnName(9))
            assertEquals("flightCapPeriod", cursor.getColumnName(10))
        }
        dbVersion3.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}",
        ).use { cursor ->
            assertEquals(0, cursor.count)
            assertEquals(3, cursor.columnCount)

            assertEquals("spocId", cursor.getColumnName(0))
            assertEquals("impressionId", cursor.getColumnName(1))
            assertEquals("impressionDateInSeconds", cursor.getColumnName(2))
        }
    }

    @Test
    fun `test1To3MigrationAddsNewSpocsAndSpocsImpressionsTables`() = runBlocking {
        // Create the database with the version 1 schema
        val dbVersion1 = helper.createDatabase(MIGRATION_TEST_DB, 1).apply {
            execSQL(
                "INSERT INTO " +
                    "'${Companion.TABLE_NAME_STORIES}' " +
                    "(url, title, imageUrl, publisher, category, timeToRead, timesShown) " +
                    "VALUES (" +
                    "'${story.url}'," +
                    "'${story.title}'," +
                    "'${story.imageUrl}'," +
                    "'${story.publisher}'," +
                    "'${story.category}'," +
                    "'${story.timeToRead}'," +
                    "'${story.timesShown}'" +
                    ")",
            )
        }
        // Validate the persisted data which will be re-checked after migration
        dbVersion1.query(
            "SELECT * FROM ${Companion.TABLE_NAME_STORIES}",
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
                ),
            )
        }

        val impression = SpocImpressionEntity(spoc.id).apply {
            impressionId = 1
            impressionDateInSeconds = 700L
        }
        // Migrate the initial database to the version 2 schema
        val dbVersion3 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB,
            3,
            true,
            Migrations.migration_1_3,
        ).apply {
            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}' (" +
                    "id, url, title, imageUrl, sponsor, clickShim, impressionShim, " +
                    "priority, lifetimeCapCount, flightCapCount, flightCapPeriod" +
                    ") VALUES (" +
                    "'${spoc.id}'," +
                    "'${spoc.url}'," +
                    "'${spoc.title}'," +
                    "'${spoc.imageUrl}'," +
                    "'${spoc.sponsor}'," +
                    "'${spoc.clickShim}'," +
                    "'${spoc.impressionShim}'," +
                    "'${spoc.priority}'," +
                    "'${spoc.lifetimeCapCount}'," +
                    "'${spoc.flightCapCount}'," +
                    "'${spoc.flightCapPeriod}'" +
                    ")",
            )

            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}' (" +
                    "spocId, impressionId, impressionDateInSeconds" +
                    ") VALUES (" +
                    "'${impression.spocId}'," +
                    "'${impression.impressionId}'," +
                    "'${impression.impressionDateInSeconds}'" +
                    ")",
            )
        }
        // Re-check the initial data we had
        dbVersion3.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_STORIES}",
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
                ),
            )
        }
        // Finally validate that the new spocs are persisted successfully
        dbVersion3.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}",
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(spoc.id, cursor.getInt(0))
            assertEquals(spoc.url, cursor.getString(1))
            assertEquals(spoc.title, cursor.getString(2))
            assertEquals(spoc.imageUrl, cursor.getString(3))
            assertEquals(spoc.sponsor, cursor.getString(4))
            assertEquals(spoc.clickShim, cursor.getString(5))
            assertEquals(spoc.impressionShim, cursor.getString(6))
            assertEquals(spoc.priority, cursor.getInt(7))
            assertEquals(spoc.lifetimeCapCount, cursor.getInt(8))
            assertEquals(spoc.flightCapCount, cursor.getInt(9))
            assertEquals(spoc.flightCapPeriod, cursor.getInt(10))
        }
        // And that the impression was also persisted successfully
        dbVersion3.query(
            "SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}",
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(impression.spocId, cursor.getInt(0))
            assertEquals(impression.impressionId, cursor.getInt(1))
            assertEquals(impression.impressionDateInSeconds, cursor.getLong(2))
        }
    }

    @Test
    fun `test3To4MigrationAddsNewIndexKeepsOldDataAndAllowsNewData`() = runBlocking {
        // Create the database with the version 3 schema
        val dbVersion3 = helper.createDatabase(MIGRATION_TEST_DB, 3).apply {
            execSQL(
                "INSERT INTO " +
                    "'${Companion.TABLE_NAME_STORIES}' " +
                    "(url, title, imageUrl, publisher, category, timeToRead, timesShown) " +
                    "VALUES (" +
                    "'${story.url}'," +
                    "'${story.title}'," +
                    "'${story.imageUrl}'," +
                    "'${story.publisher}'," +
                    "'${story.category}'," +
                    "'${story.timeToRead}'," +
                    "'${story.timesShown}'" +
                    ")",
            )
            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}' (" +
                    "id, url, title, imageUrl, sponsor, clickShim, impressionShim, " +
                    "priority, lifetimeCapCount, flightCapCount, flightCapPeriod" +
                    ") VALUES (" +
                    "'${spoc.id}'," +
                    "'${spoc.url}'," +
                    "'${spoc.title}'," +
                    "'${spoc.imageUrl}'," +
                    "'${spoc.sponsor}'," +
                    "'${spoc.clickShim}'," +
                    "'${spoc.impressionShim}'," +
                    "'${spoc.priority}'," +
                    "'${spoc.lifetimeCapCount}'," +
                    "'${spoc.flightCapCount}'," +
                    "'${spoc.flightCapPeriod}'" +
                    ")",
            )
            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}' (" +
                    "spocId, impressionId, impressionDateInSeconds" +
                    ") VALUES (" +
                    "${spoc.id}, 0, 1" +
                    ")",
            )
            // Add a new impression of the same spoc to test proper the index uniqueness
            execSQL(
                "INSERT INTO " +
                    "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}' (" +
                    "spocId, impressionId, impressionDateInSeconds" +
                    ") VALUES (" +
                    "${spoc.id}, 1, 2" +
                    ")",
            )
        }

        // Validate the data before migration
        dbVersion3.query(
            "SELECT * FROM ${Companion.TABLE_NAME_STORIES}",
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
                ),
            )
        }
        dbVersion3.query(
            "SELECT * FROM ${Companion.TABLE_NAME_SPOCS}",
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(
                spoc,
                SpocEntity(
                    id = cursor.getInt(0),
                    url = cursor.getString(1),
                    title = cursor.getString(2),
                    imageUrl = cursor.getString(3),
                    sponsor = cursor.getString(4),
                    clickShim = cursor.getString(5),
                    impressionShim = cursor.getString(6),
                    priority = cursor.getInt(7),
                    lifetimeCapCount = cursor.getInt(8),
                    flightCapCount = cursor.getInt(9),
                    flightCapPeriod = cursor.getInt(10),
                ),
            )
        }
        dbVersion3.query(
            "SELECT * FROM ${Companion.TABLE_NAME_SPOCS_IMPRESSIONS}",
        ).use { cursor ->
            assertEquals(2, cursor.count)

            cursor.moveToFirst()
            assertEquals(spoc.id, cursor.getInt(0))
            cursor.moveToNext()
            assertEquals(spoc.id, cursor.getInt(0))
        }

        // Migrate to v4 database
        val dbVersion4 = helper.runMigrationsAndValidate(
            MIGRATION_TEST_DB,
            4,
            true,
            Migrations.migration_3_4,
        )

        // Check that we have the same data as before. Just that a new index was added for faster queries.
        dbVersion4.query(
            "SELECT * FROM ${Companion.TABLE_NAME_STORIES}",
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
                ),
            )
        }
        dbVersion4.query(
            "SELECT * FROM ${Companion.TABLE_NAME_SPOCS}",
        ).use { cursor ->
            assertEquals(1, cursor.count)

            cursor.moveToFirst()
            assertEquals(
                spoc,
                SpocEntity(
                    id = cursor.getInt(0),
                    url = cursor.getString(1),
                    title = cursor.getString(2),
                    imageUrl = cursor.getString(3),
                    sponsor = cursor.getString(4),
                    clickShim = cursor.getString(5),
                    impressionShim = cursor.getString(6),
                    priority = cursor.getInt(7),
                    lifetimeCapCount = cursor.getInt(8),
                    flightCapCount = cursor.getInt(9),
                    flightCapPeriod = cursor.getInt(10),
                ),
            )
        }
        dbVersion4.query(
            "SELECT * FROM ${Companion.TABLE_NAME_SPOCS_IMPRESSIONS}",
        ).use { cursor ->
            assertEquals(2, cursor.count)

            cursor.moveToFirst()
            assertEquals(spoc.id, cursor.getInt(0))
            cursor.moveToNext()
            assertEquals(spoc.id, cursor.getInt(0))
        }

        // After adding an index check that inserting new data works as expected
        val otherSpoc = spoc.copy(
            id = spoc.id + 2,
            url = spoc.url + "2",
            title = spoc.title + "2",
            imageUrl = spoc.imageUrl + "2",
            sponsor = spoc.sponsor + "2",
            clickShim = spoc.clickShim + "2",
            impressionShim = spoc.impressionShim + "2",
            priority = spoc.priority + 2,
            lifetimeCapCount = spoc.lifetimeCapCount - 2,
            flightCapCount = spoc.flightCapPeriod * 2,
            flightCapPeriod = spoc.flightCapPeriod / 2,
        )
        dbVersion4.execSQL(
            "INSERT INTO " +
                "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}' (" +
                "id, url, title, imageUrl, sponsor, clickShim, impressionShim, " +
                "priority, lifetimeCapCount, flightCapCount, flightCapPeriod" +
                ") VALUES (" +
                "'${otherSpoc.id}'," +
                "'${otherSpoc.url}'," +
                "'${otherSpoc.title}'," +
                "'${otherSpoc.imageUrl}'," +
                "'${otherSpoc.sponsor}'," +
                "'${otherSpoc.clickShim}'," +
                "'${otherSpoc.impressionShim}'," +
                "'${otherSpoc.priority}'," +
                "'${otherSpoc.lifetimeCapCount}'," +
                "'${otherSpoc.flightCapCount}'," +
                "'${otherSpoc.flightCapPeriod}'" +
                ")",
        )
        dbVersion4.execSQL(
            "INSERT INTO " +
                "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}' (" +
                "spocId, impressionId, impressionDateInSeconds" +
                ") VALUES (" +
                "${spoc.id}, 22, 33" +
                ")",
        )
        // Test a new spoc and a new impressions of it are properly recorded.Z
        dbVersion4.execSQL(
            "INSERT INTO " +
                "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}' (" +
                "spocId, impressionId, impressionDateInSeconds" +
                ") VALUES (" +
                "${otherSpoc.id}, 23, 34" +
                ")",
        )
        // Add a new impression of the same spoc to test proper the index uniqueness
        dbVersion4.execSQL(
            "INSERT INTO " +
                "'${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}' (" +
                "spocId, impressionId, impressionDateInSeconds" +
                ") VALUES (" +
                "${otherSpoc.id}, 24, 35" +
                ")",
        )
        dbVersion4.query(
            "SELECT * FROM ${Companion.TABLE_NAME_SPOCS} ORDER BY 'id'",
        ).use { cursor ->
            assertEquals(2, cursor.count)

            cursor.moveToFirst()
            assertEquals(
                spoc,
                SpocEntity(
                    id = cursor.getInt(0),
                    url = cursor.getString(1),
                    title = cursor.getString(2),
                    imageUrl = cursor.getString(3),
                    sponsor = cursor.getString(4),
                    clickShim = cursor.getString(5),
                    impressionShim = cursor.getString(6),
                    priority = cursor.getInt(7),
                    lifetimeCapCount = cursor.getInt(8),
                    flightCapCount = cursor.getInt(9),
                    flightCapPeriod = cursor.getInt(10),
                ),
            )

            cursor.moveToNext()
            assertEquals(
                otherSpoc,
                SpocEntity(
                    id = cursor.getInt(0),
                    url = cursor.getString(1),
                    title = cursor.getString(2),
                    imageUrl = cursor.getString(3),
                    sponsor = cursor.getString(4),
                    clickShim = cursor.getString(5),
                    impressionShim = cursor.getString(6),
                    priority = cursor.getInt(7),
                    lifetimeCapCount = cursor.getInt(8),
                    flightCapCount = cursor.getInt(9),
                    flightCapPeriod = cursor.getInt(10),
                ),
            )
        }
        dbVersion4.query(
            "SELECT * FROM ${Companion.TABLE_NAME_SPOCS_IMPRESSIONS} ORDER BY 'impressionId'",
        ).use { cursor ->
            assertEquals(5, cursor.count)

            cursor.moveToFirst()
            assertEquals(spoc.id, cursor.getInt(0))
            assertEquals(0, cursor.getInt(1))
            assertEquals(1, cursor.getInt(2))
            cursor.moveToNext()
            assertEquals(spoc.id, cursor.getInt(0))
            assertEquals(1, cursor.getInt(1))
            assertEquals(2, cursor.getInt(2))
            cursor.moveToNext()
            assertEquals(spoc.id, cursor.getInt(0))
            assertEquals(22, cursor.getInt(1))
            assertEquals(33, cursor.getInt(2))
            cursor.moveToNext()
            assertEquals(otherSpoc.id, cursor.getInt(0))
            assertEquals(23, cursor.getInt(1))
            assertEquals(34, cursor.getInt(2))
            cursor.moveToNext()
            assertEquals(otherSpoc.id, cursor.getInt(0))
            assertEquals(24, cursor.getInt(1))
            assertEquals(35, cursor.getInt(2))
        }
    }
}

private val story = PocketStoryEntity(
    title = "How to Get Rid of Black Mold Naturally",
    url = "https://getpocket.com/explore/item/how-to-get-rid-of-black-mold-naturally",
    imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fpocket-syndicated-images.s3.amazonaws.com%252Farticles%252F6757%252F1628024495_6109ae86db6cc.png",
    publisher = "Pocket",
    category = "general",
    timeToRead = 4,
    timesShown = 23,
)

private val spoc = SpocEntity(
    id = 191739319,
    url = "https://i.geistm.com/l/GC_7ReasonsKetoV2_Journiest?bcid=601c567ac5b18a0414cce1d4&bhid=624f3ea9adad7604086ac6b3&utm_content=PKT_A_7ReasonsKetoV2_Journiest_40702022_RawMeatballUGC_130Off_601c567ac5b18a0414cce1d4_624f3ea9adad7604086ac6b3&tv=su4&ct=NAT-PK-PROS-130OFF5WEEK-037&utm_medium=DB&utm_source=pocket~geistm&utm_campaign=PKT_A_7ReasonsKetoV2_Journiest_40702022_RawMeatballUGC_130Off",
    title = "Eating Keto Has Never Been So Easy With Green Chef",
    imageUrl = "https://img-getpocket.cdn.mozilla.net/direct?url=realUrl.png&resize=w618-h310",
    sponsor = "Green Chef",
    clickShim = "193815086ClickShim",
    impressionShim = "193815086ImpressionShim",
    priority = 3,
    lifetimeCapCount = 50,
    flightCapCount = 10,
    flightCapPeriod = 86400,
)
