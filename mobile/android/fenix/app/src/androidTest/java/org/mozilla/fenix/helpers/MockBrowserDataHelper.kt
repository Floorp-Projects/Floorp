/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.content.Context
import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.generator.DefaultIconGenerator
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.PageVisit
import mozilla.components.concept.storage.VisitType
import mozilla.components.feature.search.ext.createSearchEngine
import okhttp3.mockwebserver.MockWebServer
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.search.SearchEngineSource.None.searchEngine

object MockBrowserDataHelper {
    val context: Context = InstrumentationRegistry.getInstrumentation().targetContext

    /**
     * Adds a new bookmark item, visible in the Bookmarks folder.
     *
     * @param url The URL of the bookmark item to add. URLs should use the "https://example.com" format.
     * @param title The title of the bookmark item to add.
     * @param position Example for the position param: 1u, 2u, etc.
     */
    fun createBookmarkItem(url: String, title: String, position: UInt?) {
        Log.i(TAG, "createBookmarkItem: Trying to add bookmark item at position: $position, with url: $url, and with title: $title")
        runBlocking {
            PlacesBookmarksStorage(context)
                .addItem(
                    BookmarkRoot.Mobile.id,
                    url,
                    title,
                    position,
                )
        }
        Log.i(TAG, "createBookmarkItem: Added bookmark item at position: $position, with url: $url, and with title: $title")
    }

    /**
     * Adds a new history item, visible in the History folder.
     *
     * @param url The URL of the history item to add. URLs should use the "https://example.com" format.
     */
    fun createHistoryItem(url: String) {
        Log.i(TAG, "createHistoryItem: Trying to add history item with url: $url")
        runBlocking {
            PlacesHistoryStorage(appContext)
                .recordVisit(
                    url,
                    PageVisit(VisitType.LINK),
                )
        }
        Log.i(TAG, "createHistoryItem: Added history item with url: $url")
    }

    /**
     * Creates a new tab with a webpage, also visible in the History folder.
     *
     * URLs should use the "https://example.com" format.
     */
    fun createTabItem(url: String) {
        Log.i(TAG, "createTabItem: Trying to create a new tab with url: $url")
        runBlocking {
            appContext.components.useCases.tabsUseCases.addTab(url)
        }
        Log.i(TAG, "createTabItem: Created a new tab with url: $url")
    }

    /**
     * Triggers a search for the provided search term in a new tab.
     *
     */
    fun createSearchHistory(searchTerm: String) {
        Log.i(TAG, "createSearchHistory: Trying to perform a new search with search term: $searchTerm")
        appContext.components.useCases.searchUseCases.newTabSearch.invoke(searchTerm)
        Log.i(TAG, "createSearchHistory: Performed a new search with search term: $searchTerm")
    }

    /**
     * Creates a new custom search engine object.
     *
     * @param mockWebServer The mockWebServer instance.
     * @param searchEngineName The name of the new search engine.
     */
    private fun createCustomSearchEngine(mockWebServer: MockWebServer, searchEngineName: String): SearchEngine {
        val searchString =
            "http://localhost:${mockWebServer.port}/pages/searchResults.html?search={searchTerms}"
        Log.i(TAG, "createCustomSearchEngine: Trying to create a custom search engine named: $searchEngineName and search string: $searchString")
        return createSearchEngine(
            name = searchEngineName,
            url = searchString,
            icon = DefaultIconGenerator().generate(appContext, IconRequest(searchString)).bitmap,
        )
    }

    /**
     * Adds a new custom search engine to the apps Search Engines list.
     *
     * @param searchEngine Use createCustomSearchEngine method to create one.
     */
    fun addCustomSearchEngine(mockWebServer: MockWebServer, searchEngineName: String) {
        val searchEngine = createCustomSearchEngine(mockWebServer, searchEngineName)
        Log.i(TAG, "addCustomSearchEngine: Trying to add a custom search engine named: $searchEngineName")
        appContext.components.useCases.searchUseCases.addSearchEngine(searchEngine)
        Log.i(TAG, "addCustomSearchEngine: Added a custom search engine named: $searchEngineName")
    }

    /**
     * Adds and selects as default a new custom search engine to the apps Search Engines list.
     *
     * @param searchEngine Use createCustomSearchEngine method to create one.
     */
    fun setCustomSearchEngine(mockWebServer: MockWebServer, searchEngineName: String) {
        val searchEngine = createCustomSearchEngine(mockWebServer, searchEngineName)
        Log.i(TAG, "setCustomSearchEngine: Trying to set a custom search engine named: $searchEngineName")
        with(appContext.components.useCases.searchUseCases) {
            addSearchEngine(searchEngine)
            selectSearchEngine(searchEngine)
        }
        Log.i(TAG, "setCustomSearchEngine: A custom search engine named: $searchEngineName was set")
    }
}
