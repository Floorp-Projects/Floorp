/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.service.pocket.PocketRecommendedStory
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.POCKET_STORIES_DEFAULT_CATEGORY_NAME
import org.mozilla.fenix.home.sessioncontrol.viewholders.pocket.PocketRecommendedStoryCategory
import kotlin.random.Random

class HomeFragmentStateTest {
    private val otherStoriesCategory =
        PocketRecommendedStoryCategory("other", getFakePocketStories(3, "other"))
    private val anotherStoriesCategory =
        PocketRecommendedStoryCategory("another", getFakePocketStories(3, "another"))
    private val defaultStoriesCategory = PocketRecommendedStoryCategory(
        POCKET_STORIES_DEFAULT_CATEGORY_NAME,
        getFakePocketStories(3)
    )

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStories is called THEN only Pocket stories from the default category are returned`() {
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory
            )
        )

        var result = homeState.getFilteredStories(2)
        assertNull(result.firstOrNull { it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME })

        result = homeState.getFilteredStories(5)
        assertNull(result.firstOrNull { it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME })
    }

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStories is called THEN no more than the indicated number of stories are returned`() {
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory
            )
        )

        // Asking for fewer than available
        var result = homeState.getFilteredStories(2)
        assertEquals(2, result.size)

        // Asking for more than available
        result = homeState.getFilteredStories(5)
        assertEquals(3, result.size)
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStories is called for fewer than in the category THEN only stories from that category are returned`() {
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory.copy(isSelected = true), anotherStoriesCategory, defaultStoriesCategory
            )
        )

        var result = homeState.getFilteredStories(2)
        assertEquals(2, result.size)
        assertNull(result.firstOrNull { it.category != otherStoriesCategory.name })

        result = homeState.getFilteredStories(3)
        assertEquals(3, result.size)
        assertNull(result.firstOrNull { it.category != otherStoriesCategory.name })
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStories is called for more than in the category THEN results topped with ones from the default category are returned`() {
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory.copy(isSelected = true), anotherStoriesCategory, defaultStoriesCategory
            )
        )

        val result = homeState.getFilteredStories(5)

        assertEquals(5, result.size)
        assertEquals(3, result.filter { it.category == otherStoriesCategory.name }.size)
        assertEquals(
            2,
            result.filter { it.category == POCKET_STORIES_DEFAULT_CATEGORY_NAME }.size
        )
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStories is called for fewer than in both THEN only stories from those categories are returned`() {
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory.copy(isSelected = true),
                anotherStoriesCategory.copy(isSelected = true),
                defaultStoriesCategory
            )
        )

        var result = homeState.getFilteredStories(2)
        assertEquals(2, result.size)
        assertNull(
            result.firstOrNull {
                it.category != otherStoriesCategory.name && it.category != anotherStoriesCategory.name
            }
        )

        result = homeState.getFilteredStories(6)
        assertEquals(6, result.size)
        assertNull(
            result.firstOrNull {
                it.category != otherStoriesCategory.name && it.category != anotherStoriesCategory.name
            }
        )
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStories is called for more than in the categories THEN results topped with ones from the default category are returned`() {
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                otherStoriesCategory.copy(isSelected = true),
                anotherStoriesCategory.copy(isSelected = true),
                defaultStoriesCategory
            )
        )

        val result = homeState.getFilteredStories(8)

        assertEquals(8, result.size)
        assertEquals(3, result.filter { it.category == otherStoriesCategory.name }.size)
        assertEquals(3, result.filter { it.category == anotherStoriesCategory.name }.size)
        assertEquals(
            2,
            result.filter { it.category == POCKET_STORIES_DEFAULT_CATEGORY_NAME }.size
        )
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStories is called for an odd number of stories THEN there are more by one stories from the newest category`() {
        val firstSelectedCategory = otherStoriesCategory.copy(lastInteractedWithTimestamp = 0, isSelected = true)
        val lastSelectedCategory = anotherStoriesCategory.copy(lastInteractedWithTimestamp = 1, isSelected = true)
        val homeState = HomeFragmentState(
            pocketStoriesCategories = listOf(
                firstSelectedCategory, lastSelectedCategory, defaultStoriesCategory
            )
        )

        val result = homeState.getFilteredStories(5)

        assertEquals(5, result.size)
        assertEquals(2, result.filter { it.category == firstSelectedCategory.name }.size)
        assertEquals(3, result.filter { it.category == lastSelectedCategory.name }.size)
    }

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStoriesCount is called THEN Pocket stories count only from the default category are returned`() {
        val availableCategories = listOf(otherStoriesCategory, defaultStoriesCategory, anotherStoriesCategory)

        var result = getFilteredStoriesCount(availableCategories, emptyList(), 2)
        assertEquals(1, result.keys.size)
        assertEquals(defaultStoriesCategory.name, result.entries.first().key)
        assertEquals(2, result[defaultStoriesCategory.name])

        result = getFilteredStoriesCount(availableCategories, emptyList(), 5)
        assertEquals(1, result.keys.size)
        assertEquals(defaultStoriesCategory.name, result.entries.first().key)
        assertEquals(3, result[defaultStoriesCategory.name])
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStoriesCount is called for at most the stories from this category THEN only stories count only from that category are returned`() {
        val availableCategories = listOf(otherStoriesCategory, defaultStoriesCategory, anotherStoriesCategory)

        var result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory), 2)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(2, result[otherStoriesCategory.name])

        result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory), 3)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(3, result[otherStoriesCategory.name])
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStoriesCount is called for more stories than this category has THEN results topped with ones from the default category are returned`() {
        val availableCategories = listOf(otherStoriesCategory, defaultStoriesCategory, anotherStoriesCategory)

        val result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory), 5)

        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    defaultStoriesCategory.name,
                    otherStoriesCategory.name
                )
            )
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(2, result[defaultStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for at most the stories count in both THEN only stories counts from those categories are returned`() {
        val availableCategories = listOf(otherStoriesCategory, defaultStoriesCategory, anotherStoriesCategory)

        var result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory, anotherStoriesCategory), 2)
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

        result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory, anotherStoriesCategory), 6)
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
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for more results than in those categories THEN results topped with ones from the default category are returned`() {
        val availableCategories = listOf(otherStoriesCategory, defaultStoriesCategory, anotherStoriesCategory)

        val result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory, anotherStoriesCategory), 8)

        assertEquals(3, result.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    defaultStoriesCategory.name,
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name
                )
            )
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(3, result[anotherStoriesCategory.name])
        assertEquals(2, result[defaultStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for an odd number of results THEN there are more by one results from first selected category`() {
        val availableCategories = listOf(otherStoriesCategory, defaultStoriesCategory, anotherStoriesCategory)

        // The lastInteractedWithTimestamp is not checked in this method but the selected categories order
        val result = getFilteredStoriesCount(availableCategories, listOf(otherStoriesCategory, anotherStoriesCategory), 5)

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
        val firstCategory = PocketRecommendedStoryCategory(
            "first", getFakePocketStories(3, "first"), true, 0
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
        val secondCategory = PocketRecommendedStoryCategory(
            "second", getFakePocketStories(3, "second"), true, 222
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

        val homeState = HomeFragmentState(pocketStoriesCategories = listOf(firstCategory, secondCategory))

        val result = homeState.getFilteredStories(6)

        assertEquals(6, result.size)
        assertSame(secondCategory.stories[2], result.first())
        assertSame(secondCategory.stories[1], result[1])
        assertSame(secondCategory.stories[0], result[2])
        assertSame(firstCategory.stories[1], result[3])
        assertSame(firstCategory.stories[0], result[4])
        assertSame(firstCategory.stories[2], result[5])
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
