/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class ReviewQualityCheckFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN feature is not enabled THEN callback returns false`() {
        var availability: Boolean? = null
        val tested = ReviewQualityCheckFeature(
            browserStore = BrowserStore(),
            shoppingExperienceFeature = FakeShoppingExperienceFeature(enabled = false),
            onAvailabilityChange = {
                availability = it
            },
        )

        tested.start()

        assertFalse(availability!!)
    }

    @Test
    fun `WHEN feature is enabled and selected tab is not a product page THEN callback returns false`() =
        runTest {
            var availability: Boolean? = null
            val tab = createTab(
                url = "https://www.mozilla.org",
                id = "test-tab",
                isProductUrl = false,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )
            val tested = ReviewQualityCheckFeature(
                browserStore = BrowserStore(
                    initialState = browserState,
                ),
                shoppingExperienceFeature = FakeShoppingExperienceFeature(),
                onAvailabilityChange = {
                    availability = it
                },
            )

            tested.start()

            assertFalse(availability!!)
        }

    @Test
    fun `WHEN feature is enabled and selected tab is a product page THEN callback returns true`() =
        runTest {
            var availability: Boolean? = null
            val tab = createTab(
                url = "https://www.mozilla.org",
                id = "test-tab",
                isProductUrl = true,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )
            val tested = ReviewQualityCheckFeature(
                browserStore = BrowserStore(
                    initialState = browserState,
                ),
                shoppingExperienceFeature = FakeShoppingExperienceFeature(),
                onAvailabilityChange = {
                    availability = it
                },
            )

            tested.start()

            assertTrue(availability!!)
        }

    @Test
    fun `WHEN feature is enabled and selected tab is switched to a product page THEN callback returns true`() =
        runTest {
            var availability: Boolean? = null
            val tab1 = createTab(
                url = "https://www.mozilla.org",
                id = "tab1",
                isProductUrl = false,
            )
            val tab2 = createTab(
                url = "https://www.shopping.org",
                id = "tab2",
                isProductUrl = true,
            )
            val browserStore = BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab1, tab2),
                    selectedTabId = tab1.id,
                ),
            )
            val tested = ReviewQualityCheckFeature(
                browserStore = browserStore,
                shoppingExperienceFeature = FakeShoppingExperienceFeature(),
                onAvailabilityChange = {
                    availability = it
                },
            )

            tested.start()
            assertFalse(availability!!)

            browserStore.dispatch(TabListAction.SelectTabAction(tab2.id)).joinBlocking()

            assertTrue(availability!!)
        }

    @Test
    fun `WHEN feature is enabled and selected tab is switched to not a product page THEN callback returns false`() =
        runTest {
            var availability: Boolean? = null
            val tab1 = createTab(
                url = "https://www.shopping.org",
                id = "tab1",
                isProductUrl = true,
            )
            val tab2 = createTab(
                url = "https://www.mozilla.org",
                id = "tab2",
                isProductUrl = false,
            )
            val browserStore = BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab1, tab2),
                    selectedTabId = tab1.id,
                ),
            )
            val tested = ReviewQualityCheckFeature(
                browserStore = browserStore,
                shoppingExperienceFeature = FakeShoppingExperienceFeature(),
                onAvailabilityChange = {
                    availability = it
                },
            )

            tested.start()
            assertTrue(availability!!)

            browserStore.dispatch(TabListAction.SelectTabAction(tab2.id)).joinBlocking()

            assertFalse(availability!!)
        }

    @Test
    fun `WHEN feature is enabled and selected tab is switched to a product page after stop is called THEN callback is only called once with false`() =
        runTest {
            var availability: Boolean? = null
            var availabilityCount = 0
            val tab1 = createTab(
                url = "https://www.mozilla.org",
                id = "tab1",
                isProductUrl = false,
            )
            val tab2 = createTab(
                url = "https://www.shopping.org",
                id = "tab2",
                isProductUrl = true,
            )
            val browserStore = BrowserStore(
                initialState = BrowserState(
                    tabs = listOf(tab1, tab2),
                    selectedTabId = tab1.id,
                ),
            )

            val tested = ReviewQualityCheckFeature(
                browserStore = browserStore,
                shoppingExperienceFeature = FakeShoppingExperienceFeature(),
                onAvailabilityChange = {
                    availability = it
                    availabilityCount++
                },
            )

            tested.start()

            tested.stop()
            browserStore.dispatch(TabListAction.SelectTabAction(tab2.id)).joinBlocking()

            assertEquals(1, availabilityCount)
            assertFalse(availability!!)
        }
}

class FakeShoppingExperienceFeature(
    private val enabled: Boolean = true,
) : ShoppingExperienceFeature {

    override val isEnabled: Boolean
        get() = enabled
}
