/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections

import android.content.Context
import androidx.arch.core.executor.testing.InstantTaskExecutorRule
import androidx.paging.PagedList
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.feature.tab.collections.db.TabCollectionDatabase
import mozilla.components.feature.tab.collections.db.TabEntity
import mozilla.components.support.ktx.java.io.truncateDirectory
import mozilla.components.support.test.fakes.engine.FakeEngine
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

@ExperimentalCoroutinesApi // for runTest
@Suppress("LargeClass") // Large test is large
class TabCollectionStorageTest {
    private lateinit var context: Context
    private lateinit var storage: TabCollectionStorage
    private lateinit var executor: ExecutorService

    @get:Rule
    var instantTaskExecutorRule = InstantTaskExecutorRule()

    @Before
    fun setUp() {
        executor = Executors.newSingleThreadExecutor()

        context = ApplicationProvider.getApplicationContext()
        val database = Room.inMemoryDatabaseBuilder(context, TabCollectionDatabase::class.java).build()

        storage = TabCollectionStorage(context)
        storage.database = lazy { database }
    }

    @After
    fun tearDown() {
        TabEntity.getStateDirectory(context.filesDir).truncateDirectory()

        executor.shutdown()
    }

    @Test
    fun testCreatingCollections() {
        storage.createCollection("Empty")
        storage.createCollection(
            "Recipes",
            listOf(
                createTab("https://www.mozilla.org", title = "Mozilla"),
                createTab("https://www.firefox.com", title = "Firefox"),
            ),
        )

        val collections = getAllCollections()

        assertEquals(2, collections.size)

        assertEquals("Recipes", collections[0].title)
        assertEquals(2, collections[0].tabs.size)

        assertEquals("https://www.firefox.com", collections[0].tabs[0].url)
        assertEquals("Firefox", collections[0].tabs[0].title)
        assertEquals("https://www.mozilla.org", collections[0].tabs[1].url)
        assertEquals("Mozilla", collections[0].tabs[1].title)

        assertEquals("Empty", collections[1].title)
        assertEquals(0, collections[1].tabs.size)
    }

    @Test
    fun testAddingTabsToExistingCollection() {
        storage.createCollection("Articles")
        var id: Long?

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(0, collections[0].tabs.size)

            id = storage.addTabsToCollection(
                collections[0],
                listOf(
                    createTab("https://www.mozilla.org", title = "Mozilla"),
                    createTab("https://www.firefox.com", title = "Firefox"),
                ),
            )
        }

        getAllCollections().let { collections ->
            assertEquals(1L, id)
            assertEquals(1, collections.size)
            assertEquals(2, collections[0].tabs.size)

            assertEquals("https://www.firefox.com", collections[0].tabs[0].url)
            assertEquals("Firefox", collections[0].tabs[0].title)
            assertEquals("https://www.mozilla.org", collections[0].tabs[1].url)
            assertEquals("Mozilla", collections[0].tabs[1].title)
        }
    }

    @Test
    fun testRemovingTabsFromCollection() {
        storage.createCollection(
            "Articles",
            listOf(
                createTab("https://www.mozilla.org", title = "Mozilla"),
                createTab("https://www.firefox.com", title = "Firefox"),
            ),
        )

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(2, collections[0].tabs.size)

            storage.removeTabFromCollection(collections[0], collections[0].tabs[0])
        }

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)
            assertEquals(1, collections[0].tabs.size)

            assertEquals("https://www.mozilla.org", collections[0].tabs[0].url)
            assertEquals("Mozilla", collections[0].tabs[0].title)
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
    fun testCreatingCollectionAndRestoringState() {
        val session1 = createTab("https://www.mozilla.org", title = "Mozilla")
        val session2 = createTab("https://www.firefox.com", title = "Firefox")

        storage.createCollection("Articles", listOf(session1, session2))

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)

            val collection = collections[0]

            val sessions = collection.restore(context, FakeEngine(), restoreSessionId = true)
            assertEquals(2, sessions.size)

            // We restored the same sessions
            matches(session1, sessions[0])
            matches(session2, sessions[1])

            assertEquals(session1.id, sessions[0].state.id)
            assertEquals(session2.id, sessions[1].state.id)
        }

        getAllCollections().let { collections ->
            assertEquals(1, collections.size)

            val collection = collections[0]

            val sessions = collection.restore(context, FakeEngine(), restoreSessionId = false)
            assertEquals(2, sessions.size)

            // The sessions are not the same but contain the same data
            assertNotEquals(session1, sessions[0])
            assertNotEquals(session2, sessions[1])

            assertNotEquals(session1.id, sessions[0].state.id)
            assertNotEquals(session2.id, sessions[1].state.id)

            assertEquals(session1.content.url, sessions[0].state.url)
            assertEquals(session2.content.url, sessions[1].state.url)

            assertEquals(session1.content.title, sessions[0].state.title)
            assertEquals(session2.content.title, sessions[1].state.title)
        }
    }

    @Test
    @Suppress("ComplexMethod")
    fun testGettingCollections() = runTest {
        storage.createCollection(
            "Articles",
            listOf(
                createTab("https://www.mozilla.org", title = "Mozilla"),
            ),
        )
        storage.createCollection(
            "Recipes",
            listOf(
                createTab("https://www.firefox.com", title = "Firefox"),
            ),
        )
        storage.createCollection(
            "Books",
            listOf(
                createTab("https://www.youtube.com", title = "YouTube"),
                createTab("https://www.amazon.com", title = "Amazon"),
            ),
        )
        storage.createCollection(
            "News",
            listOf(
                createTab("https://www.google.com", title = "Google"),
                createTab("https://www.facebook.com", title = "Facebook"),
            ),
        )
        storage.createCollection(
            "Blogs",
            listOf(
                createTab("https://www.wikipedia.org", title = "Wikipedia"),
            ),
        )

        val collections = storage.getCollections().first()

        assertEquals(5, collections.size)

        with(collections[0]) {
            assertEquals("Blogs", title)
            assertEquals(1, tabs.size)
            assertEquals("https://www.wikipedia.org", tabs[0].url)
            assertEquals("Wikipedia", tabs[0].title)
        }

        with(collections[1]) {
            assertEquals("News", title)
            assertEquals(2, tabs.size)
            assertEquals("https://www.facebook.com", tabs[0].url)
            assertEquals("Facebook", tabs[0].title)
            assertEquals("https://www.google.com", tabs[1].url)
            assertEquals("Google", tabs[1].title)
        }

        with(collections[2]) {
            assertEquals("Books", title)
            assertEquals(2, tabs.size)
            assertEquals("https://www.amazon.com", tabs[0].url)
            assertEquals("Amazon", tabs[0].title)
            assertEquals("https://www.youtube.com", tabs[1].url)
            assertEquals("YouTube", tabs[1].title)
        }

        with(collections[3]) {
            assertEquals("Recipes", title)
            assertEquals(1, tabs.size)

            assertEquals("https://www.firefox.com", tabs[0].url)
            assertEquals("Firefox", tabs[0].title)
        }

        with(collections[4]) {
            assertEquals("Articles", title)
            assertEquals(1, tabs.size)

            assertEquals("https://www.mozilla.org", tabs[0].url)
            assertEquals("Mozilla", tabs[0].title)
        }
    }

    @Test
    @Suppress("ComplexMethod")
    fun testGettingCollectionsList() = runTest {
        storage.createCollection(
            "Articles",
            listOf(
                createTab("https://www.mozilla.org", title = "Mozilla"),
            ),
        )
        storage.createCollection(
            "Recipes",
            listOf(
                createTab("https://www.firefox.com", title = "Firefox"),
            ),
        )
        storage.createCollection(
            "Books",
            listOf(
                createTab("https://www.youtube.com", title = "YouTube"),
                createTab("https://www.amazon.com", title = "Amazon"),
            ),
        )
        storage.createCollection(
            "News",
            listOf(
                createTab("https://www.google.com", title = "Google"),
                createTab("https://www.facebook.com", title = "Facebook"),
            ),
        )
        storage.createCollection(
            "Blogs",
            listOf(
                createTab("https://www.wikipedia.org", title = "Wikipedia"),
            ),
        )

        val collections = storage.getCollectionsList()
        assertEquals(5, collections.size)

        with(collections[0]) {
            assertEquals("Blogs", title)
            assertEquals(1, tabs.size)
            assertEquals("https://www.wikipedia.org", tabs[0].url)
            assertEquals("Wikipedia", tabs[0].title)
        }

        with(collections[1]) {
            assertEquals("News", title)
            assertEquals(2, tabs.size)
            assertEquals("https://www.facebook.com", tabs[0].url)
            assertEquals("Facebook", tabs[0].title)
            assertEquals("https://www.google.com", tabs[1].url)
            assertEquals("Google", tabs[1].title)
        }

        with(collections[2]) {
            assertEquals("Books", title)
            assertEquals(2, tabs.size)
            assertEquals("https://www.amazon.com", tabs[0].url)
            assertEquals("Amazon", tabs[0].title)
            assertEquals("https://www.youtube.com", tabs[1].url)
            assertEquals("YouTube", tabs[1].title)
        }

        with(collections[3]) {
            assertEquals("Recipes", title)
            assertEquals(1, tabs.size)

            assertEquals("https://www.firefox.com", tabs[0].url)
            assertEquals("Firefox", tabs[0].title)
        }

        with(collections[4]) {
            assertEquals("Articles", title)
            assertEquals(1, tabs.size)

            assertEquals("https://www.mozilla.org", tabs[0].url)
            assertEquals("Mozilla", tabs[0].title)
        }
    }

    @Test
    fun testGettingTabCollectionCount() = runTest {
        assertEquals(0, storage.getTabCollectionsCount())

        storage.createCollection(
            "Articles",
            listOf(
                createTab("https://www.mozilla.org", title = "Mozilla"),
            ),
        )
        storage.createCollection(
            "Recipes",
            listOf(
                createTab("https://www.firefox.com", title = "Firefox"),
            ),
        )

        assertEquals(2, storage.getTabCollectionsCount())

        val collections = storage.getCollections().first()
        assertEquals(2, collections.size)

        storage.removeCollection(collections[0])

        assertEquals(1, storage.getTabCollectionsCount())
    }

    @Test
    fun testRemovingAllCollections() {
        storage.createCollection(
            "Articles",
            listOf(
                createTab("https://www.mozilla.org", title = "Mozilla"),
            ),
        )
        storage.createCollection(
            "Recipes",
            listOf(
                createTab("https://www.firefox.com", title = "Firefox"),
            ),
        )

        assertEquals(2, storage.getTabCollectionsCount())
        assertEquals(2, TabEntity.getStateDirectory(context.filesDir).listFiles()?.size)

        storage.removeAllCollections()

        assertEquals(0, storage.getTabCollectionsCount())
        assertEquals(0, TabEntity.getStateDirectory(context.filesDir).listFiles()?.size)
    }

    private fun getAllCollections(): List<TabCollection> {
        val dataSource = storage.getCollectionsPaged()
            .create()

        val pagedList = PagedList.Builder(dataSource, 100)
            .setNotifyExecutor(executor)
            .setFetchExecutor(executor)
            .build()

        return pagedList.toList()
    }
}

/*
class FakeEngine : Engine {
    override val version: EngineVersion
        get() = throw NotImplementedError("Not needed for test")

    override fun createView(context: Context, attrs: AttributeSet?): EngineView =
        throw UnsupportedOperationException()

    override fun createSession(private: Boolean, contextId: String?): EngineSession =
        throw UnsupportedOperationException()

    override fun createSessionState(json: JSONObject) = FakeEngineSessionState()

    override fun createSessionStateFrom(reader: JsonReader): EngineSessionState {
        reader.beginObject()
        reader.endObject()
        return FakeEngineSessionState()
    }

    override fun name(): String =
        throw UnsupportedOperationException()

    override fun speculativeConnect(url: String) =
        throw UnsupportedOperationException()

    override val profiler: Profiler?
        get() = throw NotImplementedError("Not needed for test")

    override val settings: Settings = DefaultSettings()
}

class FakeEngineSessionState : EngineSessionState {
    override fun writeTo(writer: JsonWriter) {
        writer.beginObject()
        writer.endObject()
    }
}
 */

private fun matches(state: TabSessionState, tab: RecoverableTab) {
    assertEquals(state.content.url, tab.state.url)
    assertEquals(state.content.title, tab.state.title)
    assertEquals(state.id, tab.state.id)
    assertEquals(state.parentId, tab.state.parentId)
    assertEquals(state.contextId, tab.state.contextId)
    assertEquals(state.lastAccess, tab.state.lastAccess)
    assertEquals(state.readerState, tab.state.readerState)
}
