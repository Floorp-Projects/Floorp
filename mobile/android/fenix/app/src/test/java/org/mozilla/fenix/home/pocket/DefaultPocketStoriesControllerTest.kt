/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.ext.getCurrentFlightImpressions
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Pocket
import org.mozilla.fenix.HomeActivity
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
                    pocketStoriesCategoriesSelections = selections,
                ),
            ),
        )
        val controller = DefaultPocketStoriesController(mockk(), store)
        assertNull(Pocket.homeRecsCategoryClicked.testGetValue())

        controller.handleCategoryClick(category2)
        verify(exactly = 0) { store.dispatch(AppAction.SelectPocketStoriesCategory(category2.name)) }
        verify { store.dispatch(AppAction.DeselectPocketStoriesCategory(category2.name)) }

        assertNotNull(Pocket.homeRecsCategoryClicked.testGetValue())
        val event = Pocket.homeRecsCategoryClicked.testGetValue()!!
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
                        category1,
                        category2,
                        category3,
                        category4,
                        category5,
                        category6,
                        category7,
                        oldestSelectedCategory,
                    ),
                ),
            ),
        )
        val controller = DefaultPocketStoriesController(mockk(), store)
        assertNull(Pocket.homeRecsCategoryClicked.testGetValue())

        controller.handleCategoryClick(PocketRecommendedStoriesCategory(newSelectedCategory.name))

        verify { store.dispatch(AppAction.DeselectPocketStoriesCategory(oldestSelectedCategory.name)) }
        verify { store.dispatch(AppAction.SelectPocketStoriesCategory(newSelectedCategory.name)) }

        assertNotNull(Pocket.homeRecsCategoryClicked.testGetValue())
        val event = Pocket.homeRecsCategoryClicked.testGetValue()!!
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
                        category1,
                        category2,
                        category3,
                        category4,
                        category5,
                        category6,
                        oldestSelectedCategory,
                    ),
                ),
            ),
        )
        val newSelectedCategoryName = "newSelectedCategory"
        val controller = DefaultPocketStoriesController(mockk(), store)

        controller.handleCategoryClick(PocketRecommendedStoriesCategory(newSelectedCategoryName))

        verify(exactly = 0) { store.dispatch(AppAction.DeselectPocketStoriesCategory(oldestSelectedCategory.name)) }
        verify { store.dispatch(AppAction.SelectPocketStoriesCategory(newSelectedCategoryName)) }

        assertNotNull(Pocket.homeRecsCategoryClicked.testGetValue())
        val event = Pocket.homeRecsCategoryClicked.testGetValue()!!
        assertEquals(1, event.size)
        assertTrue(event.single().extra!!.containsKey("category_name"))
        assertEquals(newSelectedCategoryName, event.single().extra!!["category_name"])
        assertTrue(event.single().extra!!.containsKey("new_state"))
        assertEquals("selected", event.single().extra!!["new_state"])
        assertTrue(event.single().extra!!.containsKey("selected_total"))
        assertEquals("7", event.single().extra!!["selected_total"])
    }

    @Test
    fun `WHEN a new recommended story is shown THEN update the State`() {
        val store = spyk(AppStore())
        val controller = DefaultPocketStoriesController(mockk(), store)
        val storyShown: PocketRecommendedStory = mockk()
        val storyGridLocation = 1 to 2

        controller.handleStoryShown(storyShown, storyGridLocation)

        verify { store.dispatch(AppAction.PocketStoriesShown(listOf(storyShown))) }
    }

    @Test
    fun `WHEN a new sponsored story is shown THEN update the State and record telemetry`() {
        val store = spyk(AppStore())
        val controller = DefaultPocketStoriesController(mockk(), store)
        val storyShown: PocketSponsoredStory = mockk {
            every { shim.click } returns "testClickShim"
            every { shim.impression } returns "testImpressionShim"
            every { id } returns 123
        }
        var wasPingSent = false
        mockkStatic("mozilla.components.service.pocket.ext.PocketStoryKt") {
            // Simulate that the story was already shown 3 times.
            every { storyShown.getCurrentFlightImpressions() } returns listOf(2L, 3L, 7L)
            // Test that the spoc ping is immediately sent with the needed data.
            Pings.spoc.testBeforeNextSubmit { reason ->
                assertEquals(storyShown.shim.impression, Pocket.spocShim.testGetValue())
                assertEquals(Pings.spocReasonCodes.impression.name, reason?.name)
                wasPingSent = true
            }

            controller.handleStoryShown(storyShown, 1 to 2)

            verify { store.dispatch(AppAction.PocketStoriesShown(listOf(storyShown))) }
            assertNotNull(Pocket.homeRecsSpocShown.testGetValue())
            assertEquals(1, Pocket.homeRecsSpocShown.testGetValue()!!.size)
            val data = Pocket.homeRecsSpocShown.testGetValue()!!.single().extra
            assertEquals("123", data?.entries?.first { it.key == "spoc_id" }?.value)
            assertEquals("1x2", data?.entries?.first { it.key == "position" }?.value)
            assertEquals("4", data?.entries?.first { it.key == "times_shown" }?.value)
            assertTrue(wasPingSent)
        }
    }

    @Test
    fun `WHEN new stories are shown THEN update the State and record telemetry`() {
        val store = spyk(AppStore())
        val controller = DefaultPocketStoriesController(mockk(), store)
        val storiesShown: List<PocketStory> = mockk()
        assertNull(Pocket.homeRecsShown.testGetValue())

        controller.handleStoriesShown(storiesShown)

        verify { store.dispatch(AppAction.PocketStoriesShown(storiesShown)) }
        assertNotNull(Pocket.homeRecsShown.testGetValue())
        assertEquals(1, Pocket.homeRecsShown.testGetValue()!!.size)
        assertNull(Pocket.homeRecsShown.testGetValue()!!.single().extra)
    }

    @Test
    fun `WHEN a recommended story is clicked THEN open that story's url using HomeActivity and record telemetry`() {
        val story = PocketRecommendedStory(
            title = "",
            url = "testLink",
            imageUrl = "",
            publisher = "",
            category = "",
            timeToRead = 0,
            timesShown = 123,
        )
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())
        assertNull(Pocket.homeRecsStoryClicked.testGetValue())

        controller.handleStoryClicked(story, 1 to 2)

        verify { homeActivity.openToBrowserAndLoad(story.url, true, BrowserDirection.FromHome) }

        assertNotNull(Pocket.homeRecsStoryClicked.testGetValue())
        val event = Pocket.homeRecsStoryClicked.testGetValue()!!
        assertEquals(1, event.size)
        assertTrue(event.single().extra!!.containsKey("position"))
        assertEquals("1x2", event.single().extra!!["position"])
        assertTrue(event.single().extra!!.containsKey("times_shown"))
        assertEquals(story.timesShown.inc().toString(), event.single().extra!!["times_shown"])
    }

    @Test
    fun `WHEN a sponsored story is clicked THEN open that story's url using HomeActivity and record telemetry`() {
        val storyClicked = PocketSponsoredStory(
            id = 7,
            title = "",
            url = "testLink",
            imageUrl = "",
            sponsor = "",
            shim = mockk {
                every { click } returns "testClickShim"
                every { impression } returns "testImpressionShim"
            },
            priority = 3,
            caps = mockk(relaxed = true),
        )
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())
        var wasPingSent = false
        assertNull(Pocket.homeRecsSpocClicked.testGetValue())
        mockkStatic("mozilla.components.service.pocket.ext.PocketStoryKt") {
            // Simulate that the story was already shown 2 times.
            every { storyClicked.getCurrentFlightImpressions() } returns listOf(2L, 3L)
            // Test that the spoc ping is immediately sent with the needed data.
            Pings.spoc.testBeforeNextSubmit { reason ->
                assertEquals(storyClicked.shim.click, Pocket.spocShim.testGetValue())
                assertEquals(Pings.spocReasonCodes.click.name, reason?.name)
                wasPingSent = true
            }

            controller.handleStoryClicked(storyClicked, 2 to 3)

            verify { homeActivity.openToBrowserAndLoad(storyClicked.url, true, BrowserDirection.FromHome) }
            assertNotNull(Pocket.homeRecsSpocClicked.testGetValue())
            assertEquals(1, Pocket.homeRecsSpocClicked.testGetValue()!!.size)
            val data = Pocket.homeRecsSpocClicked.testGetValue()!!.single().extra
            assertEquals("7", data?.entries?.first { it.key == "spoc_id" }?.value)
            assertEquals("2x3", data?.entries?.first { it.key == "position" }?.value)
            assertEquals("3", data?.entries?.first { it.key == "times_shown" }?.value)
            assertTrue(wasPingSent)
        }
    }

    @Test
    fun `WHEN discover more is clicked then open that using HomeActivity and record telemetry`() {
        val link = "http://getpocket.com/explore"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())
        assertNull(Pocket.homeRecsDiscoverClicked.testGetValue())

        controller.handleDiscoverMoreClicked(link)

        verify { homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome) }
        assertNotNull(Pocket.homeRecsDiscoverClicked.testGetValue())
        assertEquals(1, Pocket.homeRecsDiscoverClicked.testGetValue()!!.size)
        assertNull(Pocket.homeRecsDiscoverClicked.testGetValue()!!.single().extra)
    }

    @Test
    fun `WHEN learn more is clicked then open that using HomeActivity and record telemetry`() {
        val link = "https://www.mozilla.org/en-US/firefox/pocket/"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())
        assertNull(Pocket.homeRecsLearnMoreClicked.testGetValue())

        controller.handleLearnMoreClicked(link)

        verify { homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome) }
        assertNotNull(Pocket.homeRecsLearnMoreClicked.testGetValue())
        assertNull(Pocket.homeRecsLearnMoreClicked.testGetValue()!!.single().extra)
    }

    @Test
    fun `WHEN a story is clicked THEN its link is opened`() {
        val story = PocketRecommendedStory("", "url", "", "", "", 0, 0)
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())

        controller.handleStoryClicked(story, 1 to 2)

        verifyOrder {
            homeActivity.openToBrowserAndLoad(story.url, true, BrowserDirection.FromHome)
        }
    }

    @Test
    fun `WHEN discover more is clicked THEN its link is opened`() {
        val link = "https://discoverMore.link"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())

        controller.handleDiscoverMoreClicked(link)

        verifyOrder {
            homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        }
    }

    @Test
    fun `WHEN learn more link is clicked THEN that link is opened`() {
        val link = "https://learnMore.link"
        val homeActivity: HomeActivity = mockk(relaxed = true)
        val controller = DefaultPocketStoriesController(homeActivity, mockk())

        controller.handleLearnMoreClicked(link)

        verifyOrder {
            homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        }
    }
}
