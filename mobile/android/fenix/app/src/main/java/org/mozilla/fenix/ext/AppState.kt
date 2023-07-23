/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.ext.hasFlightImpressionsLimitReached
import mozilla.components.service.pocket.ext.hasLifetimeImpressionsLimitReached
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.home.blocklist.BlocklistHandler
import org.mozilla.fenix.home.pocket.POCKET_STORIES_DEFAULT_CATEGORY_NAME
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketStory
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.utils.Settings

/**
 * Total count of all stories to show irrespective of their type.
 * This is an optimistic value taking into account that fewer than this stories may actually be available.
 */
@VisibleForTesting
internal const val POCKET_STORIES_TO_SHOW_COUNT = 8

/**
 * Total count of all sponsored Pocket stories to show.
 * This is an optimistic value taking into account that fewer than this stories may actually be available.
 */
@VisibleForTesting
internal const val POCKET_SPONSORED_STORIES_TO_SHOW_COUNT = 2

/**
 * Get the list of stories to be displayed based on the user selected categories.
 *
 * @return a list of [PocketStory]es from the currently selected categories.
 */
fun AppState.getFilteredStories(): List<PocketStory> {
    val recommendedStories = when (pocketStoriesCategoriesSelections.isEmpty()) {
        true -> {
            pocketStoriesCategories
                .find { it.name == POCKET_STORIES_DEFAULT_CATEGORY_NAME }
                ?.stories
                ?.sortedBy { it.timesShown }
                ?.take(POCKET_STORIES_TO_SHOW_COUNT) ?: emptyList()
        }
        false -> {
            val oldestSortedCategories = pocketStoriesCategoriesSelections
                .sortedByDescending { it.selectionTimestamp }
                .mapNotNull { selectedCategory ->
                    pocketStoriesCategories.find {
                        it.name == selectedCategory.name
                    }
                }

            val filteredStoriesCount = getFilteredStoriesCount(
                oldestSortedCategories,
                POCKET_STORIES_TO_SHOW_COUNT,
            )

            oldestSortedCategories
                .flatMap { category ->
                    category.stories
                        .sortedBy { it.timesShown }
                        .take(filteredStoriesCount[category.name]!!)
                }.take(POCKET_STORIES_TO_SHOW_COUNT)
        }
    }

    val sponsoredStories = getFilteredSponsoredStories(
        stories = pocketSponsoredStories,
        limit = POCKET_SPONSORED_STORIES_TO_SHOW_COUNT,
    )

    return combineRecommendedAndSponsoredStories(
        recommendedStories = recommendedStories,
        sponsoredStories = sponsoredStories,
    )
}

/**
 * Combine all available Pocket recommended and sponsored stories to show at max [POCKET_STORIES_TO_SHOW_COUNT]
 * stories of both types but based on a specific split.
 */
@VisibleForTesting
internal fun combineRecommendedAndSponsoredStories(
    recommendedStories: List<PocketRecommendedStory>,
    sponsoredStories: List<PocketSponsoredStory>,
): List<PocketStory> {
    val recommendedStoriesToShow =
        POCKET_STORIES_TO_SHOW_COUNT - sponsoredStories.size.coerceAtMost(
            POCKET_SPONSORED_STORIES_TO_SHOW_COUNT,
        )

    // Sponsored stories should be shown at position 2 and 8. If possible.
    return recommendedStories.take(1) +
        sponsoredStories.take(1) +
        recommendedStories.take(recommendedStoriesToShow).drop(1) +
        sponsoredStories.take(2).drop(1)
}

/**
 * Get how many stories needs to be shown from each currently selected category.
 *
 * @param selectedCategories ordered list of categories from which to return results.
 * @param neededStoriesCount how many stories are intended to be displayed.
 * This impacts the results by guaranteeing an even spread of stories from each category in that stories count.
 *
 * @return a mapping of how many stories are to be shown from each category from [selectedCategories].
 */
@VisibleForTesting
@Suppress("ReturnCount", "NestedBlockDepth")
internal fun getFilteredStoriesCount(
    selectedCategories: List<PocketRecommendedStoriesCategory>,
    neededStoriesCount: Int,
): Map<String, Int> {
    val totalStoriesInFilteredCategories = selectedCategories.fold(0) { availableStories, category ->
        availableStories + category.stories.size
    }

    when (totalStoriesInFilteredCategories > neededStoriesCount) {
        true -> {
            val storiesCountFromEachCategory = mutableMapOf<String, Int>()
            var currentFilteredStoriesCount = 0

            for (i in 0 until selectedCategories.maxOf { it.stories.size }) {
                selectedCategories.forEach { category ->
                    if (category.stories.getOrNull(i) != null) {
                        storiesCountFromEachCategory[category.name] =
                            storiesCountFromEachCategory[category.name]?.inc() ?: 1

                        if (++currentFilteredStoriesCount == neededStoriesCount) {
                            return storiesCountFromEachCategory
                        }
                    }
                }
            }
        }
        false -> {
            return selectedCategories.associate { it.name to it.stories.size }
        }
    }

    return emptyMap()
}

/**
 * Handle pacing and rotation of sponsored stories.
 */
@VisibleForTesting
internal fun getFilteredSponsoredStories(
    stories: List<PocketSponsoredStory>,
    limit: Int,
): List<PocketSponsoredStory> {
    return stories.asSequence()
        .filterNot { it.hasLifetimeImpressionsLimitReached() }
        .sortedByDescending { it.priority }
        .filterNot { it.hasFlightImpressionsLimitReached() }
        .take(limit)
        .toList()
}

/**
 * Filter a [AppState] by the blocklist.
 *
 * @param blocklistHandler The handler that will filter the state.
 */
fun AppState.filterState(blocklistHandler: BlocklistHandler): AppState =
    with(blocklistHandler) {
        copy(
            recentBookmarks = recentBookmarks.filteredByBlocklist(),
            recentTabs = recentTabs.filteredByBlocklist().filterContile(),
            recentHistory = recentHistory.filteredByBlocklist().filterContile(),
            recentSyncedTabState = recentSyncedTabState.filteredByBlocklist().filterContile(),
        )
    }

/**
 * Determines whether a recent tab section should be shown, based on user preference
 * and the availability of local or Synced tabs.
 */
fun AppState.shouldShowRecentTabs(settings: Settings): Boolean {
    val hasTab = recentTabs.isNotEmpty() || recentSyncedTabState is RecentSyncedTabState.Success
    return settings.showRecentTabsFeature && hasTab
}

/**
 * Determines whether a recent synced tab section should be shown, based on the availability of Synced tabs.
 */
fun AppState.shouldShowRecentSyncedTabs(): Boolean {
    return recentSyncedTabState is RecentSyncedTabState.Success
}
