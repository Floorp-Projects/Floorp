/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections

import android.content.Context
import android.util.AttributeSet
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.feature.tab.collections.db.TabCollectionDatabase
import mozilla.components.support.android.test.awaitValue
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class TabCollectionStorageTest {
    private lateinit var sessionManager: SessionManager
    private lateinit var storage: TabCollectionStorage
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        val context: Context = ApplicationProvider.getApplicationContext()
        val database = Room.inMemoryDatabaseBuilder(context, TabCollectionDatabase::class.java).build()

        val engine = FakeEngine()

        sessionManager = SessionManager(engine)
        storage = TabCollectionStorage(context, sessionManager)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        executor.shutdown()
    }

    @Test
    fun testCreatingCollections() {
        storage.createCollection("Empty")
        storage.createCollection("Recipes", listOf(
            Session("https://www.mozilla.org").apply { title = "Mozilla" },
            Session("https://www.firefox.com").apply { title = "Firefox" }
        ))

        val collections = getAllCollections()

        assertEquals(2, collections.size)

        assertEquals("Recipes", collections[0].title)
        assertEquals(2, collections[0].tabs.size)

        assertEquals("https://www.mozilla.org", collections[0].tabs[0].url)
        assertEquals("Mozilla", collections[0].tabs[0].title)
        assertEquals("https://www.firefox.com", collections[0].tabs[1].url)
        assertEquals("Firefox", collections[0].tabs[1].title)

        assertEquals("Empty", collections[1].title)
        assertEquals(0, collections[1].tabs.size)
    }

    @Test
    fun testAddingTabsToExistingCollection() {
        storage.createCollection("Articles")

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(0, collections[0].tabs.size)

            storage.addTabsToCollection(collections[0], listOf(
                Session("https://www.mozilla.org").apply { title = "Mozilla" },
                Session("https://www.firefox.com").apply { title = "Firefox" }
            ))
        }

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(2, collections[0].tabs.size)

            assertEquals("https://www.mozilla.org", collections[0].tabs[0].url)
            assertEquals("Mozilla", collections[0].tabs[0].title)
            assertEquals("https://www.firefox.com", collections[0].tabs[1].url)
            assertEquals("Firefox", collections[0].tabs[1].title)
        }
    }

    @Test
    fun testRemovingTabsFromCollection() {
        storage.createCollection("Articles", listOf(
            Session("https://www.mozilla.org").apply { title = "Mozilla" },
            Session("https://www.firefox.com").apply { title = "Firefox" }
        ))

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(2, collections[0].tabs.size)

            storage.removeTabFromCollection(collections[0], collections[0].tabs[0])
        }

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(1, collections[0].tabs.size)

            assertEquals("https://www.firefox.com", collections[0].tabs[0].url)
            assertEquals("Firefox", collections[0].tabs[0].title)
        }
    }

    @Test
    fun testRenamingCollection() {
        storage.createCollection("Articles")

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            storage.renameCollection(collections[0], "Blog Articles")
        }

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals("Blog Articles", collections[0].title)
        }
    }

    @Test
    fun testRemovingCollection() {
        storage.createCollection("Articles")
        storage.createCollection("Recipes")

        getAllCollections().let { collections ->
            assertEquals(2, collections.size)
            assertEquals("Recipes", collections[0].title)
            assertEquals("Articles", collections[1].title)

            storage.removeCollection(collections[0])
        }

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)

            assertEquals("Articles", collections[0].title)
        }
    }

    @Test
    @Suppress("ComplexMethod")
    fun testGettingCollectionsWithLimit() {
        storage.createCollection(
            "Articles", listOf(
                Session("https://www.mozilla.org").apply { title = "Mozilla" }
            )
        )
        storage.createCollection(
            "Recipes", listOf(
                Session("https://www.firefox.com").apply { title = "Firefox" }
            )
        )
        storage.createCollection(
            "Books", listOf(
                Session("https://www.youtube.com").apply { title = "YouTube" },
                Session("https://www.amazon.com").apply { title = "Amazon" }
            )
        )
        storage.createCollection(
            "News", listOf(
                Session("https://www.google.com").apply { title = "Google" },
                Session("https://www.facebook.com").apply { title = "Facebook" }
            )
        )
        storage.createCollection(
            "Blogs", listOf(
                Session("https://www.wikipedia.org").apply { title = "Wikipedia" }
            )
        )

        val collections = storage.getCollections(limit = 4)
            .awaitValue()

        assertNotNull(collections!!)
        assertEquals(4, collections.size)

        with(collections[0]) {
            assertEquals("Blogs", title)
            assertEquals(1, tabs.size)
            assertEquals("https://www.wikipedia.org", tabs[0].url)
            assertEquals("Wikipedia", tabs[0].title)
        }

        with(collections[1]) {
            assertEquals("News", title)
            assertEquals(2, tabs.size)
            assertEquals("https://www.google.com", tabs[0].url)
            assertEquals("Google", tabs[0].title)
            assertEquals("https://www.facebook.com", tabs[1].url)
            assertEquals("Facebook", tabs[1].title)
        }

        with(collections[2]) {
            assertEquals("Books", title)
            assertEquals(2, tabs.size)
            assertEquals("https://www.youtube.com", tabs[0].url)
            assertEquals("YouTube", tabs[0].title)
            assertEquals("https://www.amazon.com", tabs[1].url)
            assertEquals("Amazon", tabs[1].title)
        }

        with(collections[3]) {
            assertEquals("Recipes", title)
            assertEquals(1, tabs.size)

            assertEquals("https://www.firefox.com", tabs[0].url)
            assertEquals("Firefox", tabs[0].title)
        }
    }

    private fun getAllCollections(): List<TabCollection> {
        // Not sure exactly why; but without delaying here `getAllCollections` may not see the latest state.
        runBlocking { delay(500) }

        val dataSource = storage.getCollectionsPaged()
            .create()

        val pagedList = PagedList.Builder(dataSource, 100)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        return pagedList.toList()
    }
}

class FakeEngine : Engine {
    override fun createView(context: Context, attrs: AttributeSet?): EngineView =
        throw UnsupportedOperationException()

    override fun createSession(private: Boolean): EngineSession =
        throw UnsupportedOperationException()

    override fun createSessionState(json: JSONObject) =
        throw UnsupportedOperationException()

    override fun name(): String =
        throw UnsupportedOperationException()

    override fun speculativeConnect(url: String) =
        throw UnsupportedOperationException()

    override val settings: Settings = DefaultSettings()
}
