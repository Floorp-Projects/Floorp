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
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Pocket
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class) // For gleanTestRule
class DefaultPocketStoriesControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `GIVEN a category is selected WHEN that same category is clicked THEN deselect it and record telemetry`() {
        val category1 = PocketRecommendedStoriesCategory("cat1", emptyList())
        val category2 = PocketRecommendedStoriesCategory("cat2", emptyList())
        val selections = listOf(PocketRecommendedStoriesSelectedCategory(category2.name))
        val store = spyk(
            AppStore(
                AppState(
                    pocketStoriesCategories = listOf(category1, category2),
                    pocketStoriesCategoriesSelections = selections
                )
            )
        )
        val controller = DefaultPocketStoriesController(mockk(), store, mockk())
        assertFalse(Pocket.homeRecsCategoryClicked.testHasValue())

        controller.handleCategoryClick(category2)
        verify(exactly = 0) { store.dispatch(AppAction.SelectPocketStoriesCategory(category2.name)) }
        verify { store.dispatch(AppAction.DeselectPocketStoriesCategory(category2.name)) }

        assertTrue(Pocket.homeRecsCategoryClicked.testHasValue())
        val event = Pocket.homeRecsCategoryClicked.testGetValue()
        assertEquals(1, event.size)
        assertTrue(event.single().extra!!.containsKey("category_name"))
        assertEquals(category2.name, event.single().extra!!["category_name"])
        assertTrue(event.single().extra!!.containsKey("new_state"))
        assertEquals("deselected", event.single().extra!!["new_state"])
        assertTrue(event.single().extra!!.containsKey("selected_total"))
        assertEquals("1", event.single().extra!!["selected_total"])
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
            AppStore(
                AppState(
                    pocketStoriesCategoriesSelections = listOf(
                        category1, category2, category3, category4, category5, category6, category7, oldestSelectedCategory
                    )
                )
            )
        )
        val controller = DefaultPocketStoriesController(mockk(), store, mockk())
        assertFalse(Pocket.homeRecsCategoryClicked.testHasValue())

        controller.handleCategoryClick(PocketRecommendedStoriesCategory(newSelectedCategory.name))

        verify { store.dispatch(AppAction.DeselectPocketStoriesCategory(oldestSelectedCategory.name)) }
        verify { store.dispatch(AppAction.SelectPocketStoriesCategory(newSelectedCategory.name)) }

        assertTrue(Pocket.homeRecsCategoryClicked.testHasValue())
        val event = Pocket.homeRecsCategoryClicked.testGetValue()
        assertEquals(1, event.size)
        assertTrue(event.single().extra!!.containsKey("category_name"))
        assertEquals(newSelectedCategory.name, event.single().extra!!["category_name"])
        assertTrue(event.single().extra!!.containsKey("new_state"))
        assertEquals("selected", event.single().extra!!["new_state"])
        assertTrue(event.single().extra!!.containsKey("selected_total"))
        assertEquals("8", event.single().extra!!["selected_total"])
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
            AppStore(
                AppState(
                    pocketStoriesCategoriesSelections = listOf(
                        category1, category2, category3, category4, category5, category6, oldestSelectedCategory
                    )
                )
            )
        )
        val newSelectedCategoryName = "newSelectedCategory"
        val controller = DefaultPocketStoriesController(mockk(), store, mockk())

        controller.handleCategoryClick(PocketRecommendedStoriesCategory(newSelectedCategoryName))

        verify(exactly = 0) { store.dispatch(AppAction.DeselectPocketStoriesCategory(oldestSelectedCategory.name)) }
        verify { store.dispatch(AppAction.SelectPocketStoriesCategory(newSelectedCategoryName)) }

        assertTrue(Pocket.homeRecsCategoryClicked.testHasValue())
        val event = Pocket.homeRecsCategoryClicked.testGetValue()
        assertEquals(1, event.size)
        assertTrue(event.single().extra!!.containsKey("category_name"))
        assertEquals(newSelectedCategoryName, event.single().extra!!["category_name"])
        assertTrue(event.single().extra!!.containsKey("new_state"))
        assertEquals("selected", event.single().extra!!["new_state"])
        assertTrue(event.single().extra!!.containsKey("selected_total"))
        assertEquals("7", event.single().extra!!["selected_total"])
    }

    @Test
    fun `WHEN new stories are shown THEN update the State and record telemetry`() {
        val store = spyk(AppStore())
        val controller = DefaultPocketStoriesController(mockk(), store, mockk())
        val storiesShown: List<PocketRecommendedStory> = mockk()
        assertFalse(Pocket.homeRecsShown.testHasValue())

        controller.handleStoriesShown(storiesShown)

        verify { store.dispatch(AppAction.PocketStoriesShown(storiesShown)) }
        assertTrue(Pocket.homeRecsShown.testHasValue())
        assertEquals(1, Pocket.homeRecsShown.testGetValue().size)
        assertNull(Pocket.homeRecsShown.testGetValue().single().extra)
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
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), mockk(relaxed = true))
        assertFalse(Pocket.homeRecsStoryClicked.testHasValue())

        controller.handleStoryClicked(story, 1 to 2)

        verify { homeActivity.openToBrowserAndLoad(story.url, true, BrowserDirection.FromHome) }

        assertTrue(Pocket.homeRecsStoryClicked.testHasValue())
        val event = Pocket.homeRecsStoryClicked.testGetValue()
        assertEquals(1, event.size)
        assertTrue(event.single().extra!!.containsKey("position"))
        assertEquals("1x2", event.single().extra!!["position"])
        assertTrue(event.single().extra!!.containsKey("times_shown"))
        assertEquals(story.timesShown.inc().toString(), event.single().extra!!["times_shown"])
    }

    @Test
    fun `WHEN discover more is clicked then open that using HomeActivity and record telemetry`() {
        val link = "http://getpocket.com/explore"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), mockk(relaxed = true))
        assertFalse(Pocket.homeRecsDiscoverClicked.testHasValue())

        controller.handleDiscoverMoreClicked(link)

        verify { homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome) }
        assertTrue(Pocket.homeRecsDiscoverClicked.testHasValue())
        assertEquals(1, Pocket.homeRecsDiscoverClicked.testGetValue().size)
        assertNull(Pocket.homeRecsDiscoverClicked.testGetValue().single().extra)
    }

    @Test
    fun `WHEN learn more is clicked then open that using HomeActivity and record telemetry`() {
        val link = "https://www.mozilla.org/en-US/firefox/pocket/"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), mockk(relaxed = true))
        assertFalse(Pocket.homeRecsLearnMoreClicked.testHasValue())

        controller.handleLearnMoreClicked(link)

        verify { homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome) }
        assertTrue(Pocket.homeRecsLearnMoreClicked.testHasValue())
        assertNull(Pocket.homeRecsLearnMoreClicked.testGetValue().single().extra)
    }

    @Test
    fun `WHEN a story is clicked THEN search is dismissed and then its link opened`() {
        val story = PocketRecommendedStory("", "url", "", "", "", 0, 0)
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), navController)

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
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), navController)

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
        val controller = DefaultPocketStoriesController(homeActivity, mockk(), navController)

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
        val controller = DefaultPocketStoriesController(mockk(), mockk(), navController)

        controller.dismissSearchDialogIfDisplayed()

        verify { navController.navigateUp() }
    }

    @Test
    fun `GIVEN search dialog is not currently focused WHEN dismissSearchDialogIfDisplayed is called THEN do nothing`() {
        val navController: NavController = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(mockk(), mockk(), navController)

        controller.dismissSearchDialogIfDisplayed()

        verify(exactly = 0) { navController.navigateUp() }
    }
}
