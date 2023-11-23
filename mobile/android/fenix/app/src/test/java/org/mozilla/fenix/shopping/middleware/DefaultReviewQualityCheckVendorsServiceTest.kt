/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.ProductVendor

class DefaultReviewQualityCheckVendorsServiceTest {

    @Test
    fun `WHEN selected tab is an amazon_com page THEN amazon is first in product vendors list`() =
        runTest {
            val tab = createTab(
                url = "https://www.amazon.com/product",
                id = "test-tab",
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(
                ProductVendor.AMAZON,
                ProductVendor.BEST_BUY,
                ProductVendor.WALMART,
            )

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is a walmart page THEN walmart is first in product vendors list`() =
        runTest {
            val tab = createTab(
                url = "https://www.walmart.com/product",
                id = "test-tab",
            )

            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(
                ProductVendor.WALMART,
                ProductVendor.AMAZON,
                ProductVendor.BEST_BUY,
            )

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is a best buy page THEN best buy is first in product vendors list`() =
        runTest {
            val tab = createTab(
                url = "https://www.bestbuy.com/product",
                id = "test-tab",
            )

            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(
                ProductVendor.BEST_BUY,
                ProductVendor.AMAZON,
                ProductVendor.WALMART,
            )

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is a not a vendor page THEN default product vendors list is returned`() =
        runTest {
            val tab = createTab(
                url = "https://www.shopping.xyz/product",
                id = "test-tab",
            )

            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(
                ProductVendor.AMAZON,
                ProductVendor.BEST_BUY,
                ProductVendor.WALMART,
            )

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is a not a valid uri THEN default product vendors list is returned`() =
        runTest {
            val tab = createTab(
                url = "not a url",
                id = "test-tab",
            )

            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(
                ProductVendor.AMAZON,
                ProductVendor.BEST_BUY,
                ProductVendor.WALMART,
            )

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is an amazon_de page THEN amazon is first in product vendors list`() =
        runTest {
            val tab = createTab(
                url = "https://www.amazon.de/product",
                id = "test-tab",
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(ProductVendor.AMAZON)

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is an amazon_fr page THEN amazon is first in product vendors list`() =
        runTest {
            val tab = createTab(
                url = "https://www.amazon.fr/product",
                id = "test-tab",
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(ProductVendor.AMAZON)

            assertEquals(expected, actual)
        }

    @Test
    fun `WHEN selected tab is an amazon_in page THEN default product vendors list is returned`() =
        runTest {
            val tab = createTab(
                url = "https://www.amazon.in/product",
                id = "test-tab",
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckVendorsService(BrowserStore(browserState))

            val actual = tested.productVendors()
            val expected = listOf(
                ProductVendor.AMAZON,
                ProductVendor.BEST_BUY,
                ProductVendor.WALMART,
            )

            assertEquals(expected, actual)
        }
}
