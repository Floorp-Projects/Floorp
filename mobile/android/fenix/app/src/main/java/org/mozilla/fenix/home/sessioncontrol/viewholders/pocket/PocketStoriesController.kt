/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentStore
import mozilla.components.lib.state.Store
import mozilla.components.service.pocket.PocketRecommendedStory

/**
 * Contract for how all user interactions with the Pocket recommended stories feature are to be handled.
 */
interface PocketStoriesController {
    /**
     * Callback allowing to handle a specific [PocketRecommendedStoryCategory] being clicked by the user.
     *
     * @param categoryClicked the just clicked [PocketRecommendedStoryCategory].
     */
    fun handleCategoryClick(categoryClicked: PocketRecommendedStoryCategory): Unit

    /**
     * Callback to decide what should happen as an effect of a new list of stories being shown.
     *
     * @param storiesShown the new list of [PocketRecommendedStory]es shown to the user.
     */
    fun handleStoriesShown(storiesShown: List<PocketRecommendedStory>)
}

/**
 * Default behavior for handling all user interactions with the Pocket recommended stories feature.
 *
 * @param homeStore [Store] from which to read the current Pocket recommendations and dispatch new actions on.
 */
internal class DefaultPocketStoriesController(
    val homeStore: HomeFragmentStore
) : PocketStoriesController {
    override fun handleCategoryClick(categoryClicked: PocketRecommendedStoryCategory) {
        val allCategories = homeStore.state.pocketStoriesCategories

        // First check whether the category is clicked to be deselected.
        if (categoryClicked.isSelected) {
            homeStore.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(categoryClicked.name))
            return
        }

        // If a new category is clicked to be selected:
        // Ensure the number of categories selected at a time is capped.
        val currentlySelectedCategoriesCount = allCategories.fold(0) { count, category ->
            if (category.isSelected) count + 1 else count
        }
        val oldestCategoryToDeselect =
            if (currentlySelectedCategoriesCount == POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT) {
                allCategories
                    .filter { it.isSelected }
                    .reduce { oldestSelected, category ->
                        when (oldestSelected.lastInteractedWithTimestamp <= category.lastInteractedWithTimestamp) {
                            true -> oldestSelected
                            false -> category
                        }
                    }
            } else {
                null
            }
        oldestCategoryToDeselect?.let {
            homeStore.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(it.name))
        }

        // Finally update the selection.
        homeStore.dispatch(HomeFragmentAction.SelectPocketStoriesCategory(categoryClicked.name))
    }

    override fun handleStoriesShown(storiesShown: List<PocketRecommendedStory>) {
        homeStore.dispatch(HomeFragmentAction.PocketStoriesShown(storiesShown))
    }
}
