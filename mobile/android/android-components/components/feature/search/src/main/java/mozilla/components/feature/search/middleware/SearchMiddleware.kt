/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.middleware

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.state.SearchState
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
 *
 * @param context The application context.
 * @param additionalBundledSearchEngineIds List of (bundled) search engine IDs that will be loaded
 * in addition to the search engines for the user's region and made available through
 * [SearchState.additionalSearchEngines] and [SearchState.additionalSearchEngines].
 */
@Suppress("LongParameterList")
class SearchMiddleware(
    context: Context,
    private val additionalBundledSearchEngineIds: List<String> = emptyList(),
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
            is SearchAction.SetRegionAction -> loadSearchEngines(context.store, action.regionState)
            is SearchAction.UpdateCustomSearchEngineAction -> saveCustomSearchEngine(action)
            is SearchAction.RemoveCustomSearchEngineAction -> removeCustomSearchEngine(action)
            is SearchAction.SelectSearchEngineAction -> updateSearchEngineSelection(action)
        }

        next(action)

        when (action) {
            is SearchAction.ShowSearchEngineAction, is SearchAction.HideSearchEngineAction ->
                updateHiddenSearchEngines(context.state.search.hiddenSearchEngines)
            is SearchAction.AddAdditionalSearchEngineAction, is SearchAction.RemoveAdditionalSearchEngineAction ->
                updateAdditionalSearchEngines(context.state.search.additionalSearchEngines)
        }
    }

    private fun loadSearchEngines(
        store: Store<BrowserState, BrowserAction>,
        region: RegionState
    ) = scope.launch {
        val regionBundle = async(ioDispatcher) { bundleStorage.load(region, coroutineContext = ioDispatcher) }
        val userChoice = async(ioDispatcher) { metadataStorage.getUserSelectedSearchEngine() }
        val customSearchEngines = async(ioDispatcher) { customStorage.loadSearchEngineList() }
        val hiddenSearchEngineIds = async(ioDispatcher) { metadataStorage.getHiddenSearchEngines() }
        val additionalSearchEngineIds = async(ioDispatcher) { metadataStorage.getAdditionalSearchEngines() }
        val allAdditionalSearchEngines = async(ioDispatcher) {
            bundleStorage.load(additionalBundledSearchEngineIds, ioDispatcher)
        }

        val hiddenSearchEngines = mutableListOf<SearchEngine>()
        val filteredRegionSearchEngines = regionBundle.await().list.filter { searchEngine ->
            if (hiddenSearchEngineIds.await().contains(searchEngine.id)) {
                hiddenSearchEngines.add(searchEngine)
                false
            } else {
                true
            }
        }

        val regionSearchEngineIds = regionBundle.await().list.map { searchEngine -> searchEngine.id }

        val action = SearchAction.SetSearchEnginesAction(
            regionSearchEngines = filteredRegionSearchEngines,
            regionDefaultSearchEngineId = regionBundle.await().defaultSearchEngineId,
            userSelectedSearchEngineId = userChoice.await()?.searchEngineId,
            userSelectedSearchEngineName = userChoice.await()?.searchEngineName,
            customSearchEngines = customSearchEngines.await(),
            hiddenSearchEngines = hiddenSearchEngines,
            additionalSearchEngines = allAdditionalSearchEngines.await().filter { searchEngine ->
                searchEngine.id in additionalSearchEngineIds.await() &&
                    searchEngine.id !in regionSearchEngineIds
            },
            additionalAvailableSearchEngines = allAdditionalSearchEngines.await().filter { searchEngine ->
                searchEngine.id !in additionalSearchEngineIds.await() &&
                    searchEngine.id !in regionSearchEngineIds
            }
        )

        store.dispatch(action)
    }

    private fun updateSearchEngineSelection(
        action: SearchAction.SelectSearchEngineAction
    ) = scope.launch {
        metadataStorage.setUserSelectedSearchEngine(
            action.searchEngineId,
            action.searchEngineName
        )
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

    private fun updateHiddenSearchEngines(
        hiddenSearchEngines: List<SearchEngine>
    ) = scope.launch {
        metadataStorage.setHiddenSearchEngines(
            hiddenSearchEngines.map { searchEngine -> searchEngine.id }
        )
    }

    private fun updateAdditionalSearchEngines(
        additionalSearchEngines: List<SearchEngine>
    ) = scope.launch {
        metadataStorage.setAdditionalSearchEngines(
            additionalSearchEngines.map { searchEngine -> searchEngine.id }
        )
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
         * Loads the bundled search engines with the given [ids].
         */
        suspend fun load(
            ids: List<String>,
            coroutineContext: CoroutineContext = Dispatchers.IO
        ): List<SearchEngine>

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
         * Gets the ID (and optinally name) of the default search engine the user has picked. Returns
         * `null` if the user has not made a choice.
         */
        suspend fun getUserSelectedSearchEngine(): UserChoice?

        /**
         * Sets the ID (and optionally name) of the default search engine the user has picked.
         */
        suspend fun setUserSelectedSearchEngine(id: String, name: String?)

        /**
         * Sets the list of IDs of hidden search engines.
         */
        suspend fun setHiddenSearchEngines(ids: List<String>)

        /**
         * Gets the list of IDs of hidden search engines.
         */
        suspend fun getHiddenSearchEngines(): List<String>

        /**
         * Gets the list of IDs of additional search engines that the user explicitly added.
         */
        suspend fun getAdditionalSearchEngines(): List<String>

        /**
         * Sets the list of IDs of additional search engines that the user explicitly added.
         */
        suspend fun setAdditionalSearchEngines(ids: List<String>)

        /**
         * Data class holding the ID and name of the selected search engine of the user.
         */
        data class UserChoice(
            val searchEngineId: String,
            val searchEngineName: String?
        )
    }
}
