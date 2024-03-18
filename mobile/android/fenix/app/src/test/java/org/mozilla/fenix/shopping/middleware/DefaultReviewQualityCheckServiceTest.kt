/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.shopping.ProductAnalysis
import mozilla.components.concept.engine.shopping.ProductRecommendation
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.ProductRecommendationTestData

@RunWith(FenixRobolectricTestRunner::class)
class DefaultReviewQualityCheckServiceTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `GIVEN fetch is called WHEN onResult is invoked with the expected type THEN product analysis returns the same data`() =
        runTest {
            val engineSession = mockk<EngineSession>()
            val expected = ProductAnalysisTestData.productAnalysis()

            every {
                engineSession.requestProductAnalysis(any(), any(), any())
            }.answers {
                secondArg<(ProductAnalysis) -> Unit>().invoke(expected)
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            val actual = tested.fetchProductReview()

            assertEquals(expected, actual)
        }

    @Test
    fun `GIVEN fetch is called WHEN onException is invoked THEN product analysis returns null`() =
        runTest {
            val engineSession = mockk<EngineSession>()

            every {
                engineSession.requestProductAnalysis(any(), any(), any())
            }.answers {
                thirdArg<(Throwable) -> Unit>().invoke(RuntimeException())
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            assertNull(tested.fetchProductReview())
        }

    @Test
    fun `WHEN fetch is called THEN fetch is called for the selected tab`() = runTest {
        val engineSession = mockk<EngineSession>()
        val expected = ProductAnalysisTestData.productAnalysis()

        every {
            engineSession.requestProductAnalysis(any(), any(), any())
        }.answers {
            secondArg<(ProductAnalysis) -> Unit>().invoke(expected)
        }

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

        val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

        val actual = tested.fetchProductReview()

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN product recommendations is called WHEN onResult is invoked with the result THEN recommendations returns the data and exposure is called`() =
        runTest {
            val engineSession = mockk<EngineSession>()
            val expected = ProductRecommendationTestData.productRecommendation()
            val productRecommendations = listOf(expected)

            every {
                engineSession.requestProductRecommendations(any(), any(), any())
            }.answers {
                secondArg<(List<ProductRecommendation>) -> Unit>().invoke(productRecommendations)
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            val actual = tested.productRecommendation(false)

            assertEquals(expected, actual)
            assertNotNull(Shopping.adsExposure.testGetValue())
        }

    @Test
    fun `GIVEN product recommendations is called WHEN onResult is invoked with a empty list and telemetry should be recorded THEN recommendations returns null and no ads available event is called`() =
        runTest {
            val engineSession = mockk<EngineSession>()

            every {
                engineSession.requestProductRecommendations(any(), any(), any())
            }.answers {
                secondArg<(List<ProductRecommendation>) -> Unit>().invoke(emptyList())
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            val actual = tested.productRecommendation(true)

            assertNull(actual)
            assertNotNull(Shopping.surfaceNoAdsAvailable.testGetValue())
        }

    @Test
    fun `GIVEN product recommendations is called WHEN onResult is invoked with a empty list and telemetry should not be recorded THEN recommendations returns null and no ads available event is not called`() =
        runTest {
            val engineSession = mockk<EngineSession>()

            every {
                engineSession.requestProductRecommendations(any(), any(), any())
            }.answers {
                secondArg<(List<ProductRecommendation>) -> Unit>().invoke(emptyList())
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            val actual = tested.productRecommendation(false)

            assertNull(actual)
            assertNull(Shopping.surfaceNoAdsAvailable.testGetValue())
        }

    @Test
    fun `GIVEN product recommendations is called WHEN onException is invoked THEN recommendations returns null`() =
        runTest {
            val engineSession = mockk<EngineSession>()

            every {
                engineSession.requestProductRecommendations(any(), any(), any())
            }.answers {
                thirdArg<(Throwable) -> Unit>().invoke(RuntimeException())
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            val actual = tested.productRecommendation(false)

            assertNull(actual)
        }

    @Test
    fun `GIVEN product recommendations is called WHEN onResult is invoked with the result THEN recommendations returns the same result without re-fetching again`() =
        runTest {
            val engineSession = mockk<EngineSession>()
            val expected = ProductRecommendationTestData.productRecommendation()
            val productRecommendations = listOf(expected)

            every {
                engineSession.requestProductRecommendations(any(), any(), any())
            }.answers {
                secondArg<(List<ProductRecommendation>) -> Unit>().invoke(productRecommendations)
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            tested.productRecommendation(false)
            tested.productRecommendation(false)
            val actual = tested.productRecommendation(false)

            assertEquals(expected, actual)

            verify(exactly = 1) {
                engineSession.requestProductRecommendations(any(), any(), any())
            }
        }

    @Test
    fun `GIVEN product recommendations is called WHEN onResult is invoked with the empty result THEN recommendations fetches every time`() =
        runTest {
            val engineSession = mockk<EngineSession>()

            every {
                engineSession.requestProductRecommendations(any(), any(), any())
            }.answers {
                secondArg<(List<ProductRecommendation>) -> Unit>().invoke(emptyList())
            }

            val tab = createTab(
                url = "https://www.shopping.org/product",
                id = "test-tab",
                engineSession = engineSession,
            )
            val browserState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            )

            val tested = DefaultReviewQualityCheckService(BrowserStore(browserState))

            tested.productRecommendation(false)
            tested.productRecommendation(false)
            val actual = tested.productRecommendation(false)

            assertNull(actual)

            verify(exactly = 3) {
                engineSession.requestProductRecommendations(any(), any(), any())
            }
        }
}
