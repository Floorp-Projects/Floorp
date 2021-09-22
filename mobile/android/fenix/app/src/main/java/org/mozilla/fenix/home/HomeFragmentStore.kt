/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.graphics.Bitmap
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.getFilteredStories
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.POCKET_STORIES_TO_SHOW_COUNT
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.PocketRecommendedStoryCategory

/**
 * The [Store] for holding the [HomeFragmentState] and applying [HomeFragmentAction]s.
 */
class HomeFragmentStore(
    initialState: HomeFragmentState = HomeFragmentState(),
    middlewares: List<Middleware<HomeFragmentState, HomeFragmentAction>> = emptyList()
) : Store<HomeFragmentState, HomeFragmentAction>(
    initialState, ::homeFragmentStateReducer, middlewares
)

data class Tab(
    val sessionId: String,
    val url: String,
    val hostname: String,
    val title: String,
    val selected: Boolean? = null,
    val icon: Bitmap? = null
)

/**
 * The state for the [HomeFragment].
 *
 * @property collections The list of [TabCollection] to display in the [HomeFragment].
 * @property expandedCollections A set containing the ids of the [TabCollection] that are expanded
 *                               in the [HomeFragment].
 * @property mode The state of the [HomeFragment] UI.
 * @property topSites The list of [TopSite] in the [HomeFragment].
 * @property tip The current [Tip] to show on the [HomeFragment].
 * @property showCollectionPlaceholder If true, shows a placeholder when there are no collections.
 * @property showSetAsDefaultBrowserCard If true, shows the default browser card
 * @property recentTabs The list of recent [RecentTab] in the [HomeFragment].
 * @property recentBookmarks The list of recently saved [BookmarkNode]s to show on the [HomeFragment].
 * @property historyMetadata The list of [HistoryMetadataGroup].
 * @property pocketArticles The list of [PocketRecommendedStory].
 */
data class HomeFragmentState(
    val collections: List<TabCollection> = emptyList(),
    val expandedCollections: Set<Long> = emptySet(),
    val mode: Mode = Mode.Normal,
    val topSites: List<TopSite> = emptyList(),
    val tip: Tip? = null,
    val showCollectionPlaceholder: Boolean = false,
    val showSetAsDefaultBrowserCard: Boolean = false,
    val recentTabs: List<RecentTab> = emptyList(),
    val recentBookmarks: List<BookmarkNode> = emptyList(),
    val historyMetadata: List<HistoryMetadataGroup> = emptyList(),
    val pocketStories: List<PocketRecommendedStory> = emptyList(),
    val pocketStoriesCategories: List<PocketRecommendedStoryCategory> = emptyList()
) : State

sealed class HomeFragmentAction : Action {
    data class Change(
        val topSites: List<TopSite>,
        val mode: Mode,
        val collections: List<TabCollection>,
        val tip: Tip? = null,
        val showCollectionPlaceholder: Boolean,
        val recentTabs: List<RecentTab>,
        val recentBookmarks: List<BookmarkNode>,
        val historyMetadata: List<HistoryMetadataGroup>
    ) :
        HomeFragmentAction()

    data class CollectionExpanded(val collection: TabCollection, val expand: Boolean) :
        HomeFragmentAction()

    data class CollectionsChange(val collections: List<TabCollection>) : HomeFragmentAction()
    data class ModeChange(val mode: Mode) : HomeFragmentAction()
    data class TopSitesChange(val topSites: List<TopSite>) : HomeFragmentAction()
    data class RemoveTip(val tip: Tip) : HomeFragmentAction()
    data class RecentTabsChange(val recentTabs: List<RecentTab>) : HomeFragmentAction()
    data class RecentBookmarksChange(val recentBookmarks: List<BookmarkNode>) : HomeFragmentAction()
    data class HistoryMetadataChange(val historyMetadata: List<HistoryMetadataGroup>) : HomeFragmentAction()
    data class SelectPocketStoriesCategory(val categoryName: String) : HomeFragmentAction()
    data class DeselectPocketStoriesCategory(val categoryName: String) : HomeFragmentAction()
    data class PocketStoriesChange(val pocketStories: List<PocketRecommendedStory>) : HomeFragmentAction()
    data class PocketStoriesCategoriesChange(val storiesCategories: List<PocketRecommendedStoryCategory>) :
        HomeFragmentAction()
    object RemoveCollectionsPlaceholder : HomeFragmentAction()
    object RemoveSetDefaultBrowserCard : HomeFragmentAction()
}

@Suppress("ReturnCount")
private fun homeFragmentStateReducer(
    state: HomeFragmentState,
    action: HomeFragmentAction
): HomeFragmentState {
    return when (action) {
        is HomeFragmentAction.Change -> state.copy(
            collections = action.collections,
            mode = action.mode,
            topSites = action.topSites,
            tip = action.tip,
            recentBookmarks = action.recentBookmarks,
            recentTabs = action.recentTabs,
            historyMetadata = action.historyMetadata
        )
        is HomeFragmentAction.CollectionExpanded -> {
            val newExpandedCollection = state.expandedCollections.toMutableSet()

            if (action.expand) {
                newExpandedCollection.add(action.collection.id)
            } else {
                newExpandedCollection.remove(action.collection.id)
            }

            state.copy(expandedCollections = newExpandedCollection)
        }
        is HomeFragmentAction.CollectionsChange -> state.copy(collections = action.collections)
        is HomeFragmentAction.ModeChange -> state.copy(mode = action.mode)
        is HomeFragmentAction.TopSitesChange -> state.copy(topSites = action.topSites)
        is HomeFragmentAction.RemoveTip -> {
            state.copy(tip = null)
        }
        is HomeFragmentAction.RemoveCollectionsPlaceholder -> {
            state.copy(showCollectionPlaceholder = false)
        }
        is HomeFragmentAction.RemoveSetDefaultBrowserCard -> state.copy(showSetAsDefaultBrowserCard = false)
        is HomeFragmentAction.RecentTabsChange -> state.copy(recentTabs = action.recentTabs)
        is HomeFragmentAction.RecentBookmarksChange -> state.copy(recentBookmarks = action.recentBookmarks)
        is HomeFragmentAction.HistoryMetadataChange -> state.copy(historyMetadata = action.historyMetadata)
        is HomeFragmentAction.SelectPocketStoriesCategory -> {
            // Selecting a category means the stories to be displayed needs to also be changed.
            val updatedCategoriesState = state.copy(
                pocketStoriesCategories = state.pocketStoriesCategories.map {
                    when (it.name == action.categoryName) {
                        true -> it.copy(isSelected = true)
                        false -> it
                    }
                }
            )
            return updatedCategoriesState.copy(
                pocketStories = updatedCategoriesState.getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT)
            )
        }
        is HomeFragmentAction.DeselectPocketStoriesCategory -> {
            val updatedCategoriesState = state.copy(
                // Deselecting a category means the stories to be displayed needs to also be changed.
                pocketStoriesCategories = state.pocketStoriesCategories.map {
                    when (it.name == action.categoryName) {
                        true -> it.copy(isSelected = false)
                        false -> it
                    }
                }
            )
            return updatedCategoriesState.copy(
                pocketStories = updatedCategoriesState.getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT)
            )
        }
        is HomeFragmentAction.PocketStoriesCategoriesChange -> {
            // Whenever categories change stories to be displayed needs to also be changed.
            val updatedCategoriesState = state.copy(pocketStoriesCategories = action.storiesCategories)
            return updatedCategoriesState.copy(
                pocketStories = updatedCategoriesState.getFilteredStories(POCKET_STORIES_TO_SHOW_COUNT)
            ).also {
                println("just updated stories in the state")
            }
        }
        is HomeFragmentAction.PocketStoriesChange -> state.copy(pocketStories = action.pocketStories)
    }
}
