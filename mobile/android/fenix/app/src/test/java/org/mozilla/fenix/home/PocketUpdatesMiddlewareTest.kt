/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.datastore.core.DataStore
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStoriesService
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Test
import org.mozilla.fenix.datastore.SelectedPocketStoriesCategories
import org.mozilla.fenix.datastore.SelectedPocketStoriesCategories.SelectedPocketStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory

@ExperimentalCoroutinesApi
class PocketUpdatesMiddlewareTest {
    @Test
    fun `WHEN PocketStoriesShown is dispatched THEN update PocketStoriesService`() {
        val story1 = PocketRecommendedStory("title", "url1", "imageUrl", "publisher", "category", 0, timesShown = 0)
        val story2 = story1.copy("title2", "url2")
        val story3 = story1.copy("title3", "url3")
        val coroutineScope = TestCoroutineScope()
        val pocketService: PocketStoriesService = mockk(relaxed = true)
        val pocketMiddleware = PocketUpdatesMiddleware(coroutineScope, pocketService, mockk())
        val homeStore = HomeFragmentStore(
            HomeFragmentState(
                pocketStories = listOf(story1, story2, story3)
            ),
            listOf(pocketMiddleware)
        )

        homeStore.dispatch(HomeFragmentAction.PocketStoriesShown(listOf(story2))).joinBlocking()

        coVerify { pocketService.updateStoriesTimesShown(listOf(story2.copy(timesShown = 1))) }
    }

    @Test
    fun `WHEN persistStories is called THEN update PocketStoriesService`() {
        val stories: List<PocketRecommendedStory> = mockk()
        val pocketService: PocketStoriesService = mockk(relaxed = true)

        persistStories(
            coroutineScope = TestCoroutineScope(),
            pocketStoriesService = pocketService,
            updatedStories = stories
        )

        coVerify { pocketService.updateStoriesTimesShown(stories) }
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun `WHEN PocketStoriesCategoriesChange is dispatched THEN intercept and dispatch PocketStoriesCategoriesSelectionsChange`() {
        val persistedSelectedCategory: SelectedPocketStoriesCategory = mockk {
            every { name } returns "testCategory"
            every { selectionTimestamp } returns 123
        }
        val persistedSelectedCategories: SelectedPocketStoriesCategories = mockk {
            every { valuesList } returns mutableListOf(persistedSelectedCategory)
        }
        val dataStore: DataStore<SelectedPocketStoriesCategories> =
            mockk<FakeDataStore<SelectedPocketStoriesCategories>>(relaxed = true) {
                every { data } returns flowOf(persistedSelectedCategories)
            } as DataStore<SelectedPocketStoriesCategories>
        val currentCategories = listOf(mockk<PocketRecommendedStoriesCategory>())
        val pocketMiddleware = PocketUpdatesMiddleware(TestCoroutineScope(), mockk(), dataStore)
        val homeStore = spyk(
            HomeFragmentStore(
                HomeFragmentState(
                    pocketStoriesCategories = currentCategories
                ),
                listOf(pocketMiddleware)
            )
        )

        homeStore.dispatch(HomeFragmentAction.PocketStoriesCategoriesChange(currentCategories)).joinBlocking()

        verify {
            homeStore.dispatch(
                HomeFragmentAction.PocketStoriesCategoriesSelectionsChange(
                    storiesCategories = currentCategories,
                    categoriesSelected = listOf(
                        PocketRecommendedStoriesSelectedCategory("testCategory", 123)
                    )
                )
            )
        }
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun `WHEN SelectPocketStoriesCategory is dispatched THEN persist details in DataStore`() {
        val categ1 = PocketRecommendedStoriesCategory("categ1")
        val categ2 = PocketRecommendedStoriesCategory("categ2")
        val dataStore: DataStore<SelectedPocketStoriesCategories> =
            mockk<FakeDataStore<SelectedPocketStoriesCategories>>(relaxed = true) as
                DataStore<SelectedPocketStoriesCategories>
        val pocketMiddleware = PocketUpdatesMiddleware(TestCoroutineScope(), mockk(), dataStore)
        val homeStore = spyk(
            HomeFragmentStore(
                HomeFragmentState(
                    pocketStoriesCategories = listOf(categ1, categ2)
                ),
                listOf(pocketMiddleware)
            )
        )

        homeStore.dispatch(HomeFragmentAction.SelectPocketStoriesCategory(categ2.name)).joinBlocking()

        // Seems like the most we can test is that an update was made.
        coVerify { dataStore.updateData(any()) }
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun `WHEN DeselectPocketStoriesCategory is dispatched THEN persist details in DataStore`() {
        val categ1 = PocketRecommendedStoriesCategory("categ1")
        val categ2 = PocketRecommendedStoriesCategory("categ2")
        val dataStore: DataStore<SelectedPocketStoriesCategories> =
            mockk<FakeDataStore<SelectedPocketStoriesCategories>>(relaxed = true) as
                DataStore<SelectedPocketStoriesCategories>
        val pocketMiddleware = PocketUpdatesMiddleware(TestCoroutineScope(), mockk(), dataStore)
        val homeStore = spyk(
            HomeFragmentStore(
                HomeFragmentState(
                    pocketStoriesCategories = listOf(categ1, categ2)
                ),
                listOf(pocketMiddleware)
            )
        )

        homeStore.dispatch(HomeFragmentAction.DeselectPocketStoriesCategory(categ2.name)).joinBlocking()

        // Seems like the most we can test is that an update was made.
        coVerify { dataStore.updateData(any()) }
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun `WHEN persistCategories is called THEN update dataStore`() {
        val dataStore: DataStore<SelectedPocketStoriesCategories> =
            mockk<FakeDataStore<SelectedPocketStoriesCategories>>(relaxed = true) as
                DataStore<SelectedPocketStoriesCategories>

        persistSelectedCategories(TestCoroutineScope(), listOf(mockk(relaxed = true)), dataStore)

        // Seems like the most we can test is that an update was made.
        coVerify { dataStore.updateData(any()) }
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun `WHEN restoreSelectedCategories is called THEN dispatch PocketStoriesCategoriesSelectionsChange with data read from the persistence layer`() {
        val persistedSelectedCategory: SelectedPocketStoriesCategory = mockk {
            every { name } returns "testCategory"
            every { selectionTimestamp } returns 123
        }
        val persistedSelectedCategories: SelectedPocketStoriesCategories = mockk {
            every { valuesList } returns mutableListOf(persistedSelectedCategory)
        }
        val dataStore: DataStore<SelectedPocketStoriesCategories> =
            mockk<FakeDataStore<SelectedPocketStoriesCategories>>(relaxed = true) {
                every { data } returns flowOf(persistedSelectedCategories)
            } as DataStore<SelectedPocketStoriesCategories>
        val currentCategories = listOf(mockk<PocketRecommendedStoriesCategory>())
        val homeStore = spyk(
            HomeFragmentStore(HomeFragmentState())
        )

        restoreSelectedCategories(
            coroutineScope = TestCoroutineScope(),
            currentCategories = currentCategories,
            store = homeStore,
            selectedPocketCategoriesDataStore = dataStore
        )

        coVerify {
            homeStore.dispatch(
                HomeFragmentAction.PocketStoriesCategoriesSelectionsChange(
                    storiesCategories = currentCategories,
                    categoriesSelected = listOf(
                        PocketRecommendedStoriesSelectedCategory("testCategory", 123)
                    )
                )
            )
        }
    }
}

/**
 * Incomplete fake of a [DataStore].
 * Respects the [DataStore] contract with basic method implementations but needs to have mocked behavior
 * for more complex interactions.
 * Can be used as a replacement for mocks of the [DataStore] interface which might fail intermittently.
 */
private class FakeDataStore<T> : DataStore<T?> {
    override val data: Flow<T?>
        get() = flow { }

    override suspend fun updateData(transform: suspend (t: T?) -> T?): T? {
        return transform(null)
    }
}
