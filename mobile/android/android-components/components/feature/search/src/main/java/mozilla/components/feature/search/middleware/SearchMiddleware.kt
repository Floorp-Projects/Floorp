/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.middleware

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.feature.search.storage.BundledSearchEnginesStorage
import mozilla.components.feature.search.storage.CustomSearchEngineStorage
import mozilla.components.feature.search.storage.SearchMetadataStorage
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import java.util.Locale
import kotlin.coroutines.CoroutineContext

/**
 * [Middleware] implementation for loading and saving [SearchEngine]s whenever the state changes.
 */
class SearchMiddleware(
    context: Context,
    private val customStorage: CustomStorage = CustomSearchEngineStorage(context),
    private val bundleStorage: BundleStorage = BundledSearchEnginesStorage(context),
    private val metadataStorage: MetadataStorage = SearchMetadataStorage(context),
    private val ioDispatcher: CoroutineContext = Dispatchers.IO
) : Middleware<BrowserState, BrowserAction> {

    private val scope = CoroutineScope(ioDispatcher)

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        when (action) {
            InitAction -> init(context.store)
            is SearchAction.SetRegionAction -> loadRegionSearchEngines(context.store, action.regionState)
            is SearchAction.UpdateCustomSearchEngineAction -> saveCustomSearchEngine(action)
            is SearchAction.RemoveCustomSearchEngineAction -> removeCustomSearchEngine(action)
            is SearchAction.SetDefaultSearchEngineAction -> updateDefaultSearchEngine(action)
        }

        next(action)
    }

    private fun init(
        store: Store<BrowserState, BrowserAction>
    ) {
        loadMetadata(store)
        loadCustomSearchEngines(store)
    }

    private fun loadMetadata(
        store: Store<BrowserState, BrowserAction>
    ) = scope.launch {
        val id = metadataStorage.getDefaultSearchEngineId()
        if (id != null) {
            store.dispatch(SearchAction.SetDefaultSearchEngineAction(id))
        }
    }

    private fun loadCustomSearchEngines(
        store: Store<BrowserState, BrowserAction>
    ) = scope.launch {
        val searchEngines = customStorage.loadSearchEngineList()

        if (searchEngines.isNotEmpty()) {
            store.dispatch(SearchAction.SetCustomSearchEngines(
                searchEngines
            ))
        }
    }

    private fun loadRegionSearchEngines(
        store: Store<BrowserState, BrowserAction>,
        region: RegionState
    ) = scope.launch {
        val bundle = bundleStorage.load(region, coroutineContext = ioDispatcher)

        store.dispatch(SearchAction.SetRegionSearchEngines(
            searchEngines = bundle.list,
            regionDefaultSearchEngineId = bundle.defaultSearchEngineId
        ))
    }

    private fun updateDefaultSearchEngine(
        action: SearchAction.SetDefaultSearchEngineAction
    ) = scope.launch {
        if (action.searchEngineId != metadataStorage.getDefaultSearchEngineId()) {
            metadataStorage.setDefaultSearchEngineId(action.searchEngineId)
        }
    }

    private fun removeCustomSearchEngine(
        action: SearchAction.RemoveCustomSearchEngineAction
    ) = scope.launch {
        customStorage.removeSearchEngine(action.searchEngineId)
    }

    private fun saveCustomSearchEngine(
        action: SearchAction.UpdateCustomSearchEngineAction
    ) = scope.launch {
        customStorage.saveSearchEngine(action.searchEngine)
    }

    /**
     * A storage for custom search engines of the user.
     */
    interface CustomStorage {
        /**
         * Loads the list of search engines from the storage.
         */
        suspend fun loadSearchEngineList(): List<SearchEngine>

        /**
         * Removes the search engine with the specified [identifier] from the storage.
         */
        suspend fun removeSearchEngine(identifier: String)

        /**
         * Saves the given [searchEngine] to the storage. May replace an already existing search
         * engine with the same ID.
         */
        suspend fun saveSearchEngine(searchEngine: SearchEngine): Boolean
    }

    /**
     * A storage for loading bundled search engines.
     */
    interface BundleStorage {
        /**
         * Loads the bundled search engines for the given [locale] and [region].
         */
        suspend fun load(
            region: RegionState,
            locale: Locale = Locale.getDefault(),
            coroutineContext: CoroutineContext = Dispatchers.IO
        ): Bundle

        /**
         * A loaded bundle containing the list of search engines and the ID of the default for
         * the region.
         */
        data class Bundle(
            val list: List<SearchEngine>,
            val defaultSearchEngineId: String
        )
    }

    /**
     * A storage for saving additional metadata related to search.
     */
    interface MetadataStorage {
        /**
         * Gets the ID of the default search engine the user has picked. Returns `null` if the user
         * has not made a choice.
         */
        suspend fun getDefaultSearchEngineId(): String?

        /**
         * Sets the ID of the default search engine the user has picked.
         */
        suspend fun setDefaultSearchEngineId(id: String)
    }
}
