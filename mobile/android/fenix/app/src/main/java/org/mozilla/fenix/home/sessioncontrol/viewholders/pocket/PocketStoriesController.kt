/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.pocket

import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentStore
import mozilla.components.lib.state.Store
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

/**
 * Contract for how all user interactions with the Pocket recommended stories feature are to be handled.
 */
interface PocketStoriesController {
    /**
     * Callback to decide what should happen as an effect of a new list of stories being shown.
     *
     * @param storiesShown the new list of [PocketRecommendedStory]es shown to the user.
     */
    fun handleStoriesShown(storiesShown: List<PocketRecommendedStory>)

    /**
     * Callback allowing to handle a specific [PocketRecommendedStoriesCategory] being clicked by the user.
     *
     * @param categoryClicked the just clicked [PocketRecommendedStoriesCategory].
     */
    fun handleCategoryClick(categoryClicked: PocketRecommendedStoriesCategory): Unit

    /**
     * Callback for when the user clicks on a specific story.
     *
     * @param storyClicked The just clicked [PocketRecommendedStory] URL.
     * @param storyPosition `row x column` matrix representing the grid position of the clicked story.
     */
    fun handleStoryClicked(storyClicked: PocketRecommendedStory, storyPosition: Pair<Int, Int>)

    /**
     * Callback for when the "Learn more" link is clicked.
     *
     * @param link URL clicked.
     */
    fun handleLearnMoreClicked(link: String)

    /**
     * Callback for when the "Discover more" link is clicked.
     *
     * @param link URL clicked.
     */
    fun handleDiscoverMoreClicked(link: String)
}

/**
 * Default behavior for handling all user interactions with the Pocket recommended stories feature.
 *
 * @param homeActivity [HomeActivity] used to open URLs in a new tab.
 * @param homeStore [Store] from which to read the current Pocket recommendations and dispatch new actions on.
 * @param navController [NavController] used for navigation.
 */
internal class DefaultPocketStoriesController(
    private val homeActivity: HomeActivity,
    private val homeStore: HomeFragmentStore,
    private val navController: NavController,
    private val metrics: MetricController
) : PocketStoriesController {
    override fun handleStoriesShown(storiesShown: List<PocketRecommendedStory>) {
        homeStore.dispatch(HomeFragmentAction.PocketStoriesShown(storiesShown))
        metrics.track(Event.PocketHomeRecsShown)
    }

    override fun handleCategoryClick(categoryClicked: PocketRecommendedStoriesCategory) {
        val initialCategoriesSelections = homeStore.state.pocketStoriesCategoriesSelections

        // First check whether the category is clicked to be deselected.
        if (initialCategoriesSelections.map { it.name }.contains(categoryClicked.name)) {
            homeStore.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(categoryClicked.name))
            metrics.track(
                Event.PocketHomeRecsCategoryClicked(
                    categoryClicked.name,
                    initialCategoriesSelections.size,
                    false
                )
            )
            return
        }

        // If a new category is clicked to be selected:
        // Ensure the number of categories selected at a time is capped.
        val oldestCategoryToDeselect =
            if (initialCategoriesSelections.size == POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT) {
                initialCategoriesSelections.minByOrNull { it.selectionTimestamp }
            } else {
                null
            }
        oldestCategoryToDeselect?.let {
            homeStore.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(it.name))
        }

        // Finally update the selection.
        homeStore.dispatch(HomeFragmentAction.SelectPocketStoriesCategory(categoryClicked.name))

        metrics.track(
            Event.PocketHomeRecsCategoryClicked(
                categoryClicked.name,
                initialCategoriesSelections.size,
                true
            )
        )
    }

    override fun handleStoryClicked(storyClicked: PocketRecommendedStory, storyPosition: Pair<Int, Int>) {
        dismissSearchDialogIfDisplayed()
        homeActivity.openToBrowserAndLoad(storyClicked.url, true, BrowserDirection.FromHome)
        metrics.track(Event.PocketHomeRecsStoryClicked(storyClicked.timesShown.inc(), storyPosition))
    }

    override fun handleLearnMoreClicked(link: String) {
        dismissSearchDialogIfDisplayed()
        homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        metrics.track(Event.PocketHomeRecsLearnMoreClicked)
    }

    override fun handleDiscoverMoreClicked(link: String) {
        dismissSearchDialogIfDisplayed()
        homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        metrics.track(Event.PocketHomeRecsDiscoverMoreClicked)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun dismissSearchDialogIfDisplayed() {
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }
}
