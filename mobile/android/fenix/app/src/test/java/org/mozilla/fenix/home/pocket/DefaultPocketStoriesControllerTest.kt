/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.service.pocket.PocketRecommendedStory
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeFragmentStore

class DefaultPocketStoriesControllerTest {
    val metrics: MetricController = mockk(relaxed = true)

    @Test
    fun `GIVEN a category is selected WHEN that same category is clicked THEN deselect it and record telemetry`() {
        val category1 = PocketRecommendedStoriesCategory("cat1", emptyList())
        val category2 = PocketRecommendedStoriesCategory("cat2", emptyList())
        val selections = listOf(PocketRecommendedStoriesSelectedCategory(category2.name))
        val store = spyk(
            HomeFragmentStore(
                HomeFragmentState(
                    pocketStoriesCategories = listOf(category1, category2),
                    pocketStoriesCategoriesSelections = selections
                )
            )
        )
        val controller = DefaultPocketStoriesController(mockk(), store, mockk(), metrics)

        controller.handleCategoryClick(category2)
        verify(exactly = 0) { store.dispatch(HomeFragmentAction.SelectPocketStoriesCategory(category2.name)) }
        verify { store.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(category2.name)) }
        verify { metrics.track(Event.PocketHomeRecsCategoryClicked(category2.name, 1, false)) }
    }

    @Test
    fun `GIVEN 8 categories are selected WHEN when a new one is clicked THEN the oldest selected is deselected before selecting the new one and record telemetry`() {
        val category1 = PocketRecommendedStoriesSelectedCategory(name = "cat1", selectionTimestamp = 111)
        val category2 = PocketRecommendedStoriesSelectedCategory(name = "cat2", selectionTimestamp = 222)
        val category3 = PocketRecommendedStoriesSelectedCategory(name = "cat3", selectionTimestamp = 333)
        val oldestSelectedCategory = PocketRecommendedStoriesSelectedCategory(name = "oldestSelectedCategory", selectionTimestamp = 0)
        val category4 = PocketRecommendedStoriesSelectedCategory(name = "cat4", selectionTimestamp = 444)
        val category5 = PocketRecommendedStoriesSelectedCategory(name = "cat5", selectionTimestamp = 555)
        val category6 = PocketRecommendedStoriesSelectedCategory(name = "cat6", selectionTimestamp = 678)
        val category7 = PocketRecommendedStoriesSelectedCategory(name = "cat7", selectionTimestamp = 890)
        val newSelectedCategory = PocketRecommendedStoriesSelectedCategory(name = "newSelectedCategory", selectionTimestamp = 654321)
        val store = spyk(
            HomeFragmentStore(
                HomeFragmentState(
                    pocketStoriesCategoriesSelections = listOf(
                        category1, category2, category3, category4, category5, category6, category7, oldestSelectedCategory
                    )
                )
            )
        )
        val controller = DefaultPocketStoriesController(mockk(), store, mockk(), metrics)

        controller.handleCategoryClick(PocketRecommendedStoriesCategory(newSelectedCategory.name))

        verify { store.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(oldestSelectedCategory.name)) }
        verify { store.dispatch(HomeFragmentAction.SelectPocketStoriesCategory(newSelectedCategory.name)) }
        verify { metrics.track(Event.PocketHomeRecsCategoryClicked(newSelectedCategory.name, 8, true)) }
    }

    @Test
    fun `GIVEN fewer than 8 categories are selected WHEN when a new one is clicked THEN don't deselect anything but select the newly clicked category and record telemetry`() {
        val category1 = PocketRecommendedStoriesSelectedCategory(name = "cat1", selectionTimestamp = 111)
        val category2 = PocketRecommendedStoriesSelectedCategory(name = "cat2", selectionTimestamp = 222)
        val category3 = PocketRecommendedStoriesSelectedCategory(name = "cat3", selectionTimestamp = 333)
        val oldestSelectedCategory = PocketRecommendedStoriesSelectedCategory(name = "oldestSelectedCategory", selectionTimestamp = 0)
        val category4 = PocketRecommendedStoriesSelectedCategory(name = "cat4", selectionTimestamp = 444)
        val category5 = PocketRecommendedStoriesSelectedCategory(name = "cat5", selectionTimestamp = 555)
        val category6 = PocketRecommendedStoriesSelectedCategory(name = "cat6", selectionTimestamp = 678)
        val store = spyk(
            HomeFragmentStore(
                HomeFragmentState(
                    pocketStoriesCategoriesSelections = listOf(
                        category1, category2, category3, category4, category5, category6, oldestSelectedCategory
                    )
                )
            )
        )
        val newSelectedCategoryName = "newSelectedCategory"
        val controller = DefaultPocketStoriesController(mockk(), store, mockk(), metrics)

        controller.handleCategoryClick(PocketRecommendedStoriesCategory(newSelectedCategoryName))

        verify(exactly = 0) { store.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(oldestSelectedCategory.name)) }
        verify { store.dispatch(HomeFragmentAction.SelectPocketStoriesCategory(newSelectedCategoryName)) }
        verify { metrics.track(Event.PocketHomeRecsCategoryClicked(newSelectedCategoryName, 7, true)) }
    }

    @Test
    fun `WHEN new stories are shown THEN update the State and record telemetry`() {
        val store = spyk(HomeFragmentStore())
        val controller = DefaultPocketStoriesController(mockk(), store, mockk(), metrics)
        val storiesShown: List<PocketRecommendedStory> = mockk()

        controller.handleStoriesShown(storiesShown)

        verify { store.dispatch(HomeFragmentAction.PocketStoriesShown(storiesShown)) }
        verify { metrics.track(Event.PocketHomeRecsShown) }
    }

    @Test
    fun `WHEN a story is clicked then open that story's url using HomeActivity and record telemetry`() {
        val story = PocketRecommendedStory(
            title = "",
            url = "testLink",
            imageUrl = "",
            publisher = "",
            category = "",
            timeToRead = 0,
            timesShown = 123
        )
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), mockk(relaxed = true), metrics)

        controller.handleStoryClicked(story, 1 to 2)

        verify { homeActivity.openToBrowserAndLoad(story.url, true, BrowserDirection.FromHome) }
        metrics.track(Event.PocketHomeRecsStoryClicked(story.timesShown, 1 to 2))
    }

    @Test
    fun `WHEN discover more is clicked then open that using HomeActivity and record telemetry`() {
        val link = "http://getpocket.com/explore"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), mockk(relaxed = true), metrics)

        controller.handleDiscoverMoreClicked(link)

        verify { homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome) }
        metrics.track(Event.PocketHomeRecsDiscoverMoreClicked)
    }

    @Test
    fun `WHEN learn more is clicked then open that using HomeActivity and record telemetry`() {
        val link = "https://www.mozilla.org/en-US/firefox/pocket/"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), mockk(relaxed = true), metrics)

        controller.handleLearnMoreClicked(link)

        verify { homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome) }
        metrics.track(Event.PocketHomeRecsLearnMoreClicked)
    }

    @Test
    fun `WHEN a story is clicked THEN search is dismissed and then its link opened`() {
        val story = PocketRecommendedStory("", "url", "", "", "", 0, 0)
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), navController, metrics)

        controller.handleStoryClicked(story, 1 to 2)

        verifyOrder {
            navController.navigateUp()
            homeActivity.openToBrowserAndLoad(story.url, true, BrowserDirection.FromHome)
        }
    }

    @Test
    fun `WHEN discover more is clicked THEN search is dismissed and then its link opened`() {
        val link = "https://discoverMore.link"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), navController, metrics)

        controller.handleDiscoverMoreClicked(link)

        verifyOrder {
            navController.navigateUp()
            homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        }
    }

    @Test
    fun `WHEN learn more link is clicked THEN search is dismissed and then that link is opened`() {
        val link = "https://learnMore.link"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), navController, metrics)

        controller.handleLearnMoreClicked(link)

        verifyOrder {
            navController.navigateUp()
            homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        }
    }

    @Test
    fun `GIVEN search dialog is currently focused WHEN dismissSearchDialogIfDisplayed is called THEN close the search dialog`() {
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        val controller = DefaultPocketStoriesController(mockk(), mockk(), navController, mockk())

        controller.dismissSearchDialogIfDisplayed()

        verify { navController.navigateUp() }
    }

    @Test
    fun `GIVEN search dialog is not currently focused WHEN dismissSearchDialogIfDisplayed is called THEN do nothing`() {
        val navController: NavController = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(mockk(), mockk(), navController, mockk())

        controller.dismissSearchDialogIfDisplayed()

        verify(exactly = 0) { navController.navigateUp() }
    }
}
