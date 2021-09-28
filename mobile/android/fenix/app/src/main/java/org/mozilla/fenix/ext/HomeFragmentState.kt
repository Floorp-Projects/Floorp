/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.POCKET_STORIES_DEFAULT_CATEGORY_NAME
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.PocketRecommendedStoryCategory

/**
 * Get the list of stories to be displayed.
 * Either the stories from the [POCKET_STORIES_DEFAULT_CATEGORY_NAME] either
 * filtered stories based on the user selected categories.
 *
 * @param neededStoriesCount how many stories are intended to be displayed.
 * This only impacts filtered results guaranteeing an even spread of stories from each category.
 *
 * @return a list of [PocketRecommendedStory]es from the currently selected categories
 * topped if necessary with stories from the [POCKET_STORIES_DEFAULT_CATEGORY_NAME] up to [neededStoriesCount].
 */
fun HomeFragmentState.getFilteredStories(
    neededStoriesCount: Int
): List<PocketRecommendedStory> {
    val currentlySelectedCategories = pocketStoriesCategories.filter { it.isSelected }

    if (currentlySelectedCategories.isEmpty()) {
        return pocketStoriesCategories
            .find {
                it.name == POCKET_STORIES_DEFAULT_CATEGORY_NAME
            }?.stories
            ?.sortedBy { it.timesShown }
            ?.take(neededStoriesCount) ?: emptyList()
    }

    val oldestSortedCategories = currentlySelectedCategories
        .sortedByDescending { it.lastInteractedWithTimestamp }

    val filteredStoriesCount = getFilteredStoriesCount(
        pocketStoriesCategories, oldestSortedCategories, neededStoriesCount
    )

    // Add general stories at the end of the stories list to show until neededStoriesCount
    val generalStoriesTopup = filteredStoriesCount[POCKET_STORIES_DEFAULT_CATEGORY_NAME]?.let { neededTopups ->
        pocketStoriesCategories
            .find { it.name == POCKET_STORIES_DEFAULT_CATEGORY_NAME }
            ?.stories
            ?.sortedBy { it.timesShown }
            ?.take(neededTopups)
    } ?: emptyList()

    return oldestSortedCategories
        .flatMap { category ->
            category.stories.sortedBy { it.timesShown }.take(filteredStoriesCount[category.name]!!)
        }.plus(generalStoriesTopup)
        .take(neededStoriesCount)
}

/**
 * Get how many stories needs to be shown from each currently selected category.
 *
 * If the selected categories together don't have [neededStoriesCount] stories then
 * the difference is added from the [POCKET_STORIES_DEFAULT_CATEGORY_NAME] category.
 *
 * @param allCategories the list of all Pocket stories categories.
 * @param selectedCategories ordered list of categories from which to return results.
 * @param neededStoriesCount how many stories are intended to be displayed.
 * This impacts the results by guaranteeing an even spread of stories from each category in that stories count.
 *
 * @return a mapping of how many stories are to be shown from each category from [selectedCategories].
 * The result is topped with stories counts from the [POCKET_STORIES_DEFAULT_CATEGORY_NAME] up to [neededStoriesCount].
 */
@VisibleForTesting
@Suppress("ReturnCount", "NestedBlockDepth")
internal fun getFilteredStoriesCount(
    allCategories: List<PocketRecommendedStoryCategory>,
    selectedCategories: List<PocketRecommendedStoryCategory>,
    neededStoriesCount: Int
): Map<String, Int> {
    val totalStoriesInFilteredCategories = selectedCategories.fold(0) { availableStories, category ->
        availableStories + category.stories.size
    }

    when {
        totalStoriesInFilteredCategories == neededStoriesCount -> {
            return selectedCategories.map { it.name to it.stories.size }.toMap()
        }
        totalStoriesInFilteredCategories < neededStoriesCount -> {
            return selectedCategories.map { it.name to it.stories.size }.toMap() +
                allCategories.filter { it.name == POCKET_STORIES_DEFAULT_CATEGORY_NAME }.map {
                    it.name to (neededStoriesCount - totalStoriesInFilteredCategories).coerceAtMost(it.stories.size)
                }.toMap()
        }
        else -> {
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
    }

    return emptyMap()
}
