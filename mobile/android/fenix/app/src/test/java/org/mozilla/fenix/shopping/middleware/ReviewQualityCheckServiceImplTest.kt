/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.shopping.ProductAnalysis
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.doAnswer

class ReviewQualityCheckServiceImplTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN fetch is called WHEN onResult is invoked THEN product analysis returns the same data`() =
        runTest {
            val engineSession = mock<EngineSession>()
            val expected = mock<ProductAnalysis>()

            doAnswer { invocation ->
                val onResult: (ProductAnalysis) -> Unit = invocation.getArgument(1)
                onResult(expected)
            }.`when`(engineSession).requestProductAnalysis(any(), any(), any())

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = ReviewQualityCheckServiceImpl(BrowserStore(browserState))

            val actual = tested.fetchProductReview()

            assertEquals(expected, actual)
        }

    @Test
    fun `GIVEN fetch is called WHEN onException is invoked THEN product analysis returns null`() =
        runTest {
            val engineSession = mock<EngineSession>()

            doAnswer { invocation ->
                val onException: (Throwable) -> Unit = invocation.getArgument(2)
                onException(RuntimeException())
            }.`when`(engineSession).requestProductAnalysis(any(), any(), any())

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = ReviewQualityCheckServiceImpl(BrowserStore(browserState))

            assertNull(tested.fetchProductReview())
        }

    @Test
    fun `WHEN fetch is called THEN fetch is called for the selected tab`() = runTest {
        val engineSession = mock<EngineSession>()

        val expected = mock<ProductAnalysis>()
        doAnswer { invocation ->
            val onResult: (ProductAnalysis) -> Unit = invocation.getArgument(1)
            onResult(expected)
        }.`when`(engineSession).requestProductAnalysis(any(), any(), any())

        val tab1 = createTab(
            url = "https://www.mozilla.org",
            id = "1",
        )
        val tab2 = createTab(
            url = "https://www.shopping.org/product",
            id = "2",
            engineSession = engineSession,
        )
        val browserState = BrowserState(
            tabs = listOf(tab1, tab2),
            selectedTabId = tab2.id,
        )

        val tested = ReviewQualityCheckServiceImpl(BrowserStore(browserState))

        val actual = tested.fetchProductReview()

        assertEquals(expected, actual)
    }
}
