/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.mockk
import mozilla.components.service.pocket.PocketRecommendedStory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.home.pocket.POCKET_STORIES_DEFAULT_CATEGORY_NAME
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory
import org.mozilla.fenix.home.recenttabs.RecentTab
import kotlin.random.Random

class AppStateTest {
    private val otherStoriesCategory =
        PocketRecommendedStoriesCategory("other", getFakePocketStories(3, "other"))
    private val anotherStoriesCategory =
        PocketRecommendedStoriesCategory("another", getFakePocketStories(3, "another"))
    private val defaultStoriesCategory = PocketRecommendedStoriesCategory(
        POCKET_STORIES_DEFAULT_CATEGORY_NAME,
        getFakePocketStories(3)
    )

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStories is called THEN only Pocket stories from the default category are returned`() {
        val state = AppState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory
            )
        )

        var result = state.getFilteredStories(2)
        assertNull(result.firstOrNull { it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME })

        result = state.getFilteredStories(5)
        assertNull(result.firstOrNull { it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME })
    }

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStories is called THEN no more than the indicated number of stories are returned`() {
        val state = AppState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory
            )
        )

        // Asking for fewer than available
        var result = state.getFilteredStories(2)
        assertEquals(2, result.size)

        // Asking for more than available
        result = state.getFilteredStories(5)
        assertEquals(3, result.size)
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStories is called for fewer than in the category THEN only stories from that category are returned`() {
        val state = AppState(
            pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory),
            pocketStoriesCategoriesSelections = listOf(PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name))
        )

        var result = state.getFilteredStories(2)
        assertEquals(2, result.size)
        assertNull(result.firstOrNull { it.category != otherStoriesCategory.name })

        result = state.getFilteredStories(3)
        assertEquals(3, result.size)
        assertNull(result.firstOrNull { it.category != otherStoriesCategory.name })
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStories is called for fewer than in both THEN only stories from those categories are returned`() {
        val state = AppState(
            pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory),
            pocketStoriesCategoriesSelections = listOf(
                PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name)
            )
        )

        var result = state.getFilteredStories(2)
        assertEquals(2, result.size)
        assertNull(
            result.firstOrNull {
                it.category != otherStoriesCategory.name && it.category != anotherStoriesCategory.name
            }
        )

        result = state.getFilteredStories(6)
        assertEquals(6, result.size)
        assertNull(
            result.firstOrNull {
                it.category != otherStoriesCategory.name && it.category != anotherStoriesCategory.name
            }
        )
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStories is called for an odd number of stories THEN there are more by one stories from the newest category`() {
        val state = AppState(
            pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory),
            pocketStoriesCategoriesSelections = listOf(
                PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name, selectionTimestamp = 0),
                PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name, selectionTimestamp = 1)
            )
        )

        val result = state.getFilteredStories(5)

        assertEquals(5, result.size)
        assertEquals(2, result.filter { it.category == otherStoriesCategory.name }.size)
        assertEquals(3, result.filter { it.category == anotherStoriesCategory.name }.size)
    }

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStoriesCount is called THEN return an empty result`() {
        val result = getFilteredStoriesCount(emptyList(), 1)

        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStoriesCount is called for at most the stories from this category THEN only stories count only from that category are returned`() {
        var result = getFilteredStoriesCount(listOf(otherStoriesCategory), 2)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(2, result[otherStoriesCategory.name])

        result = getFilteredStoriesCount(listOf(otherStoriesCategory), 3)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(3, result[otherStoriesCategory.name])
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStoriesCount is called for more stories than in this category THEN return only that`() {
        val result = getFilteredStoriesCount(listOf(otherStoriesCategory), 4)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(3, result[otherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for at most the stories count in both THEN only stories counts from those categories are returned`() {
        var result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 2)
        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name
                )
            )
        )
        assertEquals(1, result[otherStoriesCategory.name])
        assertEquals(1, result[anotherStoriesCategory.name])

        result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 6)
        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name
                )
            )
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(3, result[anotherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for more results than stories in both THEN only stories counts from those categories are returned`() {
        val result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 8)
        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name
                )
            )
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(3, result[anotherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for an odd number of results THEN there are more by one results from first selected category`() {
        val result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 5)

        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name
                )
            )
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(2, result[anotherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories selected with more than needed stories WHEN getFilteredStories is called THEN the results are sorted in the order of least shown`() {
        val firstCategory = PocketRecommendedStoriesCategory(
            "first", getFakePocketStories(3, "first")
        ).run {
            // Avoid the first item also being the oldest to eliminate a potential bug in code
            // that would still get the expected result.
            copy(
                stories = stories.mapIndexed { index, story ->
                    when (index) {
                        0 -> story.copy(timesShown = 333)
                        1 -> story.copy(timesShown = 0)
                        else -> story.copy(timesShown = 345)
                    }
                }
            )
        }
        val secondCategory = PocketRecommendedStoriesCategory(
            "second", getFakePocketStories(3, "second")
        ).run {
            // Avoid the first item also being the oldest to eliminate a potential bug in code
            // that would still get the expected result.
            copy(
                stories = stories.mapIndexed { index, story ->
                    when (index) {
                        0 -> story.copy(timesShown = 222)
                        1 -> story.copy(timesShown = 111)
                        else -> story.copy(timesShown = 11)
                    }
                }
            )
        }

        val state = AppState(
            pocketStoriesCategories = listOf(firstCategory, secondCategory),
            pocketStoriesCategoriesSelections = listOf(
                PocketRecommendedStoriesSelectedCategory(firstCategory.name, selectionTimestamp = 0),
                PocketRecommendedStoriesSelectedCategory(secondCategory.name, selectionTimestamp = 222)
            )
        )

        val result = state.getFilteredStories(6)

        assertEquals(6, result.size)
        assertSame(secondCategory.stories[2], result.first())
        assertSame(secondCategory.stories[1], result[1])
        assertSame(secondCategory.stories[0], result[2])
        assertSame(firstCategory.stories[1], result[3])
        assertSame(firstCategory.stories[0], result[4])
        assertSame(firstCategory.stories[2], result[5])
    }

    @Test
    fun `GIVEN old selections of categories which do not exist anymore WHEN getFilteredStories is called THEN ignore not found selections`() {
        val state = AppState(
            pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory),
            pocketStoriesCategoriesSelections = listOf(
                PocketRecommendedStoriesSelectedCategory("unexistent"),
                PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name)
            )
        )

        val result = state.getFilteredStories(6)

        assertEquals(3, result.size)
        assertNull(result.firstOrNull { it.category != anotherStoriesCategory.name })
    }

    @Test
    fun `GIVEN recentTabs contains a SearchGroup WHEN recentSearchGroup is called THEN return the group`() {
        val searchGroup: RecentTab.SearchGroup = mockk()
        val normalTab: RecentTab.Tab = mockk()
        val state = AppState(recentTabs = listOf(normalTab, searchGroup))

        assertEquals(searchGroup, state.recentSearchGroup)
    }

    @Test
    fun `GIVEN recentTabs does not contains SearchGroup WHEN recentSearchGroup is called THEN return null`() {
        val normalTab1: RecentTab.Tab = mockk()
        val normalTab2: RecentTab.Tab = mockk()
        val state = AppState(recentTabs = listOf(normalTab1, normalTab2))

        assertNull(state.recentSearchGroup)
    }
}

private fun getFakePocketStories(
    limit: Int = 1,
    category: String = POCKET_STORIES_DEFAULT_CATEGORY_NAME
): List<PocketRecommendedStory> {
    return mutableListOf<PocketRecommendedStory>().apply {
        for (index in 0 until limit) {
            val randomNumber = Random.nextInt(0, 10)

            add(
                PocketRecommendedStory(
                    title = "This is a ${"very ".repeat(randomNumber)} long title",
                    publisher = "Publisher",
                    url = "https://story$randomNumber.com",
                    imageUrl = "",
                    timeToRead = randomNumber,
                    category = category,
                    timesShown = index.toLong()
                )
            )
        }
    }
}
