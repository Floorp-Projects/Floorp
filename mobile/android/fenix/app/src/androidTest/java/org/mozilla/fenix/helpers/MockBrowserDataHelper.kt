/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.content.Context
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
        runBlocking {
            PlacesBookmarksStorage(context)
                .addItem(
                    BookmarkRoot.Mobile.id,
                    url,
                    title,
                    position,
                )
        }
    }

    /**
     * Adds a new history item, visible in the History folder.
     *
     * @param url The URL of the history item to add. URLs should use the "https://example.com" format.
     */
    fun createHistoryItem(url: String) {
        runBlocking {
            PlacesHistoryStorage(appContext)
                .recordVisit(
                    url,
                    PageVisit(VisitType.LINK),
                )
        }
    }

    /**
     * Creates a new tab with a webpage, also visible in the History folder.
     *
     * URLs should use the "https://example.com" format.
     */
    fun createTabItem(url: String) {
        runBlocking {
            appContext.components.useCases.tabsUseCases.addTab(url)
        }
    }

    /**
     * Triggers a search for the provided search term in a new tab.
     *
     */
    fun createSearchHistory(searchTerm: String) {
        appContext.components.useCases.searchUseCases.newTabSearch.invoke(searchTerm)
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

        appContext.components.useCases.searchUseCases.addSearchEngine(searchEngine)
    }

    /**
     * Adds and selects as default a new custom search engine to the apps Search Engines list.
     *
     * @param searchEngine Use createCustomSearchEngine method to create one.
     */
    fun setCustomSearchEngine(mockWebServer: MockWebServer, searchEngineName: String) {
        val searchEngine = createCustomSearchEngine(mockWebServer, searchEngineName)

        with(appContext.components.useCases.searchUseCases) {
            addSearchEngine(searchEngine)
            selectSearchEngine(searchEngine)
        }
    }
}
