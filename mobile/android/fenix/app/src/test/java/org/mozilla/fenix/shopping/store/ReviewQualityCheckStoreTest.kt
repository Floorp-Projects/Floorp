/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertTrue
import kotlinx.coroutines.test.runTest
import mozilla.components.lib.state.ext.observeForever
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.HighlightsCardExpanded
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.InfoCardExpanded
import org.mozilla.fenix.components.appstate.AppAction.ShoppingAction.SettingsCardExpanded
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.shopping.ShoppingState
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.ProductRecommendationTestData
import org.mozilla.fenix.shopping.ShoppingExperienceFeature
import org.mozilla.fenix.shopping.fake.FakeNetworkChecker
import org.mozilla.fenix.shopping.fake.FakeReviewQualityCheckPreferences
import org.mozilla.fenix.shopping.fake.FakeReviewQualityCheckService
import org.mozilla.fenix.shopping.fake.FakeReviewQualityCheckVendorsService
import org.mozilla.fenix.shopping.fake.FakeShoppingExperienceFeature
import org.mozilla.fenix.shopping.middleware.AnalysisStatusDto
import org.mozilla.fenix.shopping.middleware.AnalysisStatusProgressDto
import org.mozilla.fenix.shopping.middleware.NetworkChecker
import org.mozilla.fenix.shopping.middleware.ReportBackInStockStatusDto
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckNetworkMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferences
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferencesMiddleware
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckService
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.Progress
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.ProductVendor
import java.util.Locale

class ReviewQualityCheckStoreTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `GIVEN the user has not opted in the feature WHEN store is created THEN state should display not opted in UI`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = false,
                        isProductRecommendationsEnabled = false,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        productVendors = listOf(
                            ProductVendor.BEST_BUY,
                            ProductVendor.AMAZON,
                            ProductVendor.WALMART,
                        ),
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.NotOptedIn(
                productVendors = listOf(
                    ProductVendor.BEST_BUY,
                    ProductVendor.AMAZON,
                    ProductVendor.WALMART,
                ),
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has not opted in the feature WHEN the user opts in THEN state should display opted in UI`() =
        runTest {
            var cfrConditionUpdated = false
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = false,
                        isProductRecommendationsEnabled = false,
                        updateCFRCallback = { cfrConditionUpdated = true },
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.OptIn).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
            assertEquals(true, cfrConditionUpdated)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user opts out THEN state should display not opted in UI`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.OptOut).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.NotOptedIn()
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature and product recommendations feature is disabled THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = null,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = null,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)

            // Even if toggle action is dispatched, state is not changed
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            tested.waitUntilIdle()
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature and product recommendations are off WHEN the user turns on product recommendations THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = false,
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature and product recommendations are on WHEN the user turns off product recommendations THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                        productRecommendation = { ProductRecommendationTestData.productRecommendation() },
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    recommendedProductState = ReviewQualityCheckState.RecommendedProductState.Initial,
                ),
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN product recommendations are available WHEN the user turns product recommendations off THEN state should not contain product recommendation`() =
        runTest {
            setAndResetLocale {
                var productRecommendationsFetchCounter = 0
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = true,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                            productRecommendation = {
                                productRecommendationsFetchCounter++
                                ProductRecommendationTestData.productRecommendation()
                            },
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                assertEquals(1, productRecommendationsFetchCounter)

                tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productRecommendationsPreference = false,
                    productRecommendationsExposure = true,
                    productVendor = ProductVendor.BEST_BUY,
                    productReviewState = ProductAnalysisTestData.analysisPresent(),
                )
                assertEquals(expected, tested.state)
                assertEquals(1, productRecommendationsFetchCounter)
            }
        }

    @Test
    fun `GIVEN product recommendations are available WHEN the user turns product recommendations off and then back on THEN state should contain product recommendation`() =
        runTest {
            setAndResetLocale {
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = true,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                            productRecommendation = { ProductRecommendationTestData.productRecommendation() },
                        ),
                        shoppingExperienceFeature = FakeShoppingExperienceFeature(
                            productRecommendationsExposureEnabled = true,
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productRecommendationsPreference = true,
                    productRecommendationsExposure = true,
                    productVendor = ProductVendor.BEST_BUY,
                    productReviewState = ProductAnalysisTestData.analysisPresent(
                        recommendedProductState = ProductRecommendationTestData.product(),
                    ),
                )
                assertEquals(expected, tested.state)
            }
        }

    @Test
    fun `GIVEN product recommendations are available but analysis failed WHEN the user turns product recommendations on THEN recommendations should not be fetched`() =
        runTest {
            setAndResetLocale {
                var productRecommendationsFetchCounter = 0
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = false,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { null },
                            productRecommendation = {
                                productRecommendationsFetchCounter++
                                ProductRecommendationTestData.productRecommendation()
                            },
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                assertEquals(0, productRecommendationsFetchCounter)

                tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productRecommendationsPreference = true,
                    productRecommendationsExposure = true,
                    productVendor = ProductVendor.BEST_BUY,
                    productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.GenericError,
                )
                assertEquals(expected, tested.state)
                assertEquals(0, productRecommendationsFetchCounter)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN there is existing card state data for a pdp THEN it should be restored`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(
                initialState = AppState(
                    shoppingState = ShoppingState(
                        productCardState = mapOf(
                            "pdp" to ShoppingState.CardState(
                                isHighlightsExpanded = false,
                                isSettingsExpanded = true,
                                isInfoExpanded = true,
                            ),
                        ),
                    ),
                ),
                middlewares = listOf(captureActionsMiddleware),
            )
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isHighlightsExpanded = false,
                isSettingsExpanded = true,
                isInfoExpanded = true,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user expands settings THEN state should reflect that`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(middlewares = listOf(captureActionsMiddleware))
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ExpandCollapseSettings).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            appStore.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isSettingsExpanded = true,
            )
            assertEquals(expected, tested.state)
            captureActionsMiddleware.assertFirstAction(SettingsCardExpanded::class) {
                assertEquals(SettingsCardExpanded("pdp", true), it)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user collapses settings THEN state should reflect that`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(
                initialState = AppState(
                    shoppingState = ShoppingState(
                        productCardState = mapOf(
                            "pdp" to ShoppingState.CardState(isSettingsExpanded = true),
                        ),
                    ),
                ),
                middlewares = listOf(captureActionsMiddleware),
            )
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ExpandCollapseSettings).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            appStore.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isSettingsExpanded = false,
            )
            assertEquals(expected, tested.state)
            captureActionsMiddleware.assertFirstAction(SettingsCardExpanded::class) {
                assertEquals(SettingsCardExpanded("pdp", false), it)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user expands info card THEN state should reflect that`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(middlewares = listOf(captureActionsMiddleware))
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ExpandCollapseInfo).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            appStore.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isInfoExpanded = true,
            )
            assertEquals(expected, tested.state)
            captureActionsMiddleware.assertFirstAction(InfoCardExpanded::class) {
                assertEquals(InfoCardExpanded("pdp", true), it)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user collapses info card THEN state should reflect that`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(
                initialState = AppState(
                    shoppingState = ShoppingState(
                        productCardState = mapOf(
                            "pdp" to ShoppingState.CardState(isInfoExpanded = true),
                        ),
                    ),
                ),
                middlewares = listOf(captureActionsMiddleware),
            )
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ExpandCollapseInfo).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            appStore.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isInfoExpanded = false,
            )
            assertEquals(expected, tested.state)
            captureActionsMiddleware.assertFirstAction(InfoCardExpanded::class) {
                assertEquals(InfoCardExpanded("pdp", false), it)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user expands highlights card THEN state should reflect that`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(middlewares = listOf(captureActionsMiddleware))
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ExpandCollapseHighlights).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            appStore.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isHighlightsExpanded = true,
            )
            assertEquals(expected, tested.state)

            captureActionsMiddleware.assertFirstAction(HighlightsCardExpanded::class) {
                assertEquals(HighlightsCardExpanded("pdp", true), it)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN the user collapses highlights card THEN state should reflect that`() =
        runTest {
            val captureActionsMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
            val appStore = AppStore(
                initialState = AppState(
                    shoppingState = ShoppingState(
                        productCardState = mapOf(
                            "pdp" to ShoppingState.CardState(isHighlightsExpanded = true),
                        ),
                    ),
                ),
                middlewares = listOf(captureActionsMiddleware),
            )
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                    ),
                    reviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(
                        selectedTabUrl = "pdp",
                    ),
                    appStore = appStore,
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ExpandCollapseHighlights).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            appStore.waitUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
                isHighlightsExpanded = false,
            )
            assertEquals(expected, tested.state)
            captureActionsMiddleware.assertFirstAction(HighlightsCardExpanded::class) {
                assertEquals(HighlightsCardExpanded("pdp", false), it)
            }
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN a product analysis is fetched successfully THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN a product analysis returns an error THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.GenericError,
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN the user has opted in the feature WHEN device is not connected to the internet THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(),
                    networkChecker = FakeNetworkChecker(isConnected = false),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.NetworkError,
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `WHEN reanalysis api call fails THEN state should reflect that`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        reanalysis = null,
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.GenericError,
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN a product analysis WHEN reanalysis call succeeds and status fails THEN state should reflect that`() =
        runTest {
            val productAnalysisList = listOf(
                ProductAnalysisTestData.productAnalysis(
                    needsAnalysis = true,
                    grade = "B",
                ),
                ProductAnalysisTestData.productAnalysis(),
            )

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = { null },
                        productAnalysis = { productAnalysisList[it] },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    reviewGrade = ReviewQualityCheckState.Grade.B,
                    analysisStatus = AnalysisStatus.NeedsAnalysis,
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `WHEN reanalysis and status api call succeeds THEN analysis should be fetched and displayed`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = {
                            AnalysisStatusProgressDto(
                                status = AnalysisStatusDto.COMPLETED,
                                progress = 100.0,
                            )
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN reanalysis and status api call succeeds WHEN notEnoughReviews is true THEN not enough reviews card is displayed`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(
                                notEnoughReviews = true,
                            )
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = {
                            AnalysisStatusProgressDto(
                                status = AnalysisStatusDto.COMPLETED,
                                progress = 100.0,
                            )
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.NotEnoughReviews,
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN a product analysis WHEN analysis status is in progress or pending THEN state should be updated to reanalysing`() =
        runTest {
            val statusProgressResponses = listOf(
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 50.0,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 61.6,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 92.52,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.COMPLETED,
                    progress = 100.0,
                ),
            )
            var counter = 0

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis()
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = {
                            statusProgressResponses[counter].also { counter++ }
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            val observedState = mutableListOf<ReviewQualityCheckState>()
            tested.observeForever {
                observedState.add(it)
            }

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expectedProgressState1 = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(61.6f)),
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )

            val expectedProgressState2 = expectedProgressState1.copy(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(92.52f)),
                ),
            )

            // Since reanalyzing is an intermediate state and the tests completes to get to the final
            // state, this checks if the intermediate state is present in the observed state.
            assertTrue(observedState.contains(expectedProgressState1))
            assertTrue(observedState.contains(expectedProgressState2))
        }

    @Test
    fun `GIVEN reanalyse product is clicked WHEN analysis status is in progress or pending THEN state should be updated to reanalysing`() =
        runTest {
            val statusProgressResponses = listOf(
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.OTHER,
                    progress = 0.0,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 30.0,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 61.6,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 92.52,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.COMPLETED,
                    progress = 100.0,
                ),
            )
            var counter = 0

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis()
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = {
                            statusProgressResponses[counter].also { counter++ }
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            val observedState = mutableListOf<ReviewQualityCheckState>()
            tested.observeForever {
                observedState.add(it)
            }

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expectedProgressState1 = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(30f)),
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )

            val expectedProgressState2 = expectedProgressState1.copy(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(61.6f)),
                ),
            )

            val expectedProgressState3 = expectedProgressState1.copy(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(92.52f)),
                ),
            )

            // Since reanalyzing is an intermediate state and the tests completes to get to the final
            // state, this checks if the intermediate state is present in the observed state.
            assertTrue(observedState.contains(expectedProgressState1))
            assertTrue(observedState.contains(expectedProgressState2))
            assertTrue(observedState.contains(expectedProgressState3))
        }

    @Test
    fun `GIVEN a product analysis WHEN analysis status is completed THEN state should display analysis as usual`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(needsAnalysis = true)
                        },
                        reanalysis = AnalysisStatusDto.COMPLETED,
                        statusProgress = {
                            AnalysisStatusProgressDto(
                                status = AnalysisStatusDto.COMPLETED,
                                progress = 100.0,
                            )
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            val observedState = mutableListOf<ReviewQualityCheckState>()
            tested.observeForever {
                observedState.add(it)
            }

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.NeedsAnalysis,
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)

            val notExpected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(0f)),
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertFalse(observedState.contains(notExpected))
        }

    @Test
    fun `GIVEN a product analysis WHEN analysis status fails THEN state should display analysis as usual`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis()
                        },
                        reanalysis = null,
                        statusProgress = { null },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            val observedState = mutableListOf<ReviewQualityCheckState>()
            tested.observeForever {
                observedState.add(it)
            }

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)

            val notExpected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisStatus.Reanalyzing(Progress(0f)),
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertFalse(observedState.contains(notExpected))
        }

    @Test
    fun `GIVEN no analysis WHEN analysis is in progress THEN state should update to no analysis present with progress`() =
        runTest {
            val statusProgressResponses = listOf(
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 30.0,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 61.6,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 92.52,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.COMPLETED,
                    progress = 100.0,
                ),
            )
            var counter = 0

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(
                                grade = null,
                                adjustedRating = null,
                                needsAnalysis = true,
                            )
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = {
                            statusProgressResponses[counter].also { counter++ }
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            val observedState = mutableListOf<ReviewQualityCheckState>()
            tested.observeForever {
                observedState.add(it)
            }

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expectedProgressState1 = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.noAnalysisPresent(
                    progress = 61.6f,
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )

            val expectedProgressState2 = expectedProgressState1.copy(
                productReviewState = ProductAnalysisTestData.noAnalysisPresent(
                    progress = 92.52f,
                ),
            )

            // Since reanalyzing is an intermediate state and the tests completes to get to the final
            // state, this checks if the intermediate state is present in the observed state.
            assertTrue(observedState.contains(expectedProgressState1))
            assertTrue(observedState.contains(expectedProgressState2))
        }

    @Test
    fun `GIVEN no analysis WHEN reanalyze THEN state should update to no analysis present with progress`() =
        runTest {
            val statusProgressResponses = listOf(
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.OTHER,
                    progress = 0.0,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 30.0,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 61.6,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.IN_PROGRESS,
                    progress = 92.52,
                ),
                AnalysisStatusProgressDto(
                    status = AnalysisStatusDto.COMPLETED,
                    progress = 100.0,
                ),
            )
            var counter = 0

            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(
                                grade = null,
                                adjustedRating = null,
                                needsAnalysis = true,
                            )
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = {
                            statusProgressResponses[counter].also { counter++ }
                        },
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            val observedState = mutableListOf<ReviewQualityCheckState>()
            tested.observeForever {
                observedState.add(it)
            }

            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expectedProgressState1 = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.noAnalysisPresent(
                    progress = 30f,
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )

            val expectedProgressState2 = expectedProgressState1.copy(
                productReviewState = ProductAnalysisTestData.noAnalysisPresent(
                    progress = 61.6f,
                ),
            )

            val expectedProgressState3 = expectedProgressState1.copy(
                productReviewState = ProductAnalysisTestData.noAnalysisPresent(
                    progress = 92.52f,
                ),
            )

            // Since reanalyzing is an intermediate state and the tests completes to get to the final
            // state, this checks if the intermediate state is present in the observed state.
            assertTrue(observedState.contains(expectedProgressState1))
            assertTrue(observedState.contains(expectedProgressState2))
            assertTrue(observedState.contains(expectedProgressState3))
        }

    @Test
    fun `GIVEN deletedProduct is true and deletedProductReported is false WHEN report back in stock is clicked THEN thanks for reporting card is shown`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(
                                deletedProduct = true,
                                deletedProductReported = false,
                            )
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = { null },
                        report = ReportBackInStockStatusDto.REPORT_CREATED,
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.ProductNotAvailable,
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)

            tested.dispatch(ReviewQualityCheckAction.ReportProductBackInStock).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val reportExpected = expected.copy(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.ThanksForReporting,
            )
            assertEquals(reportExpected, tested.state)
        }

    @Test
    fun `GIVEN deletedProduct is true WHEN report back in stock returns not deleted THEN state is updated to no analysis`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(
                                deletedProduct = true,
                                deletedProductReported = false,
                            )
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = { null },
                        report = ReportBackInStockStatusDto.NOT_DELETED,
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.ProductNotAvailable,
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)

            val noAnalysisTested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(isEnabled = true),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = {
                            ProductAnalysisTestData.productAnalysis(
                                grade = null,
                                adjustedRating = null,
                                needsAnalysis = true,
                            )
                        },
                        reanalysis = AnalysisStatusDto.PENDING,
                        statusProgress = { null },
                        report = ReportBackInStockStatusDto.NOT_DELETED,
                    ),
                    networkChecker = FakeNetworkChecker(isConnected = true),
                ),
            )

            noAnalysisTested.dispatch(ReviewQualityCheckAction.ReportProductBackInStock).joinBlocking()
            noAnalysisTested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val reportExpected = expected.copy(
                productReviewState = ProductAnalysisTestData.noAnalysisPresent(progress = -1f),
            )
            assertEquals(reportExpected, noAnalysisTested.state)
        }

    @Test
    fun `GIVEN product recommendations are enabled WHEN a product analysis is fetched successfully THEN product recommendation should also be fetched and displayed if available`() =
        runTest {
            setAndResetLocale {
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = true,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                            productRecommendation = { ProductRecommendationTestData.productRecommendation() },
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.waitUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productReviewState = ProductAnalysisTestData.analysisPresent(
                        recommendedProductState = ProductRecommendationTestData.product(),
                    ),
                    productRecommendationsPreference = true,
                    productRecommendationsExposure = true,
                    productVendor = ProductVendor.BEST_BUY,
                )
                assertEquals(expected, tested.state)
            }
        }

    @Test
    fun `GIVEN product recommendations are disabled WHEN a product analysis is fetched successfully and exposure is set to true THEN product recommendation should also be fetched`() =
        runTest {
            setAndResetLocale {
                var productRecommendationFetched = false
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = false,
                        ),
                        shoppingExperienceFeature = FakeShoppingExperienceFeature(
                            productRecommendationsExposureEnabled = true,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                            productRecommendation = {
                                productRecommendationFetched = true
                                ProductRecommendationTestData.productRecommendation()
                            },
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.waitUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productReviewState = ProductAnalysisTestData.analysisPresent(),
                    productRecommendationsPreference = false,
                    productRecommendationsExposure = true,
                    productVendor = ProductVendor.BEST_BUY,
                )
                assertEquals(expected, tested.state)
                assertTrue(productRecommendationFetched)
            }
        }

    @Test
    fun `GIVEN product recommendations are disabled WHEN a product analysis is fetched successfully and exposure is set to false THEN product recommendation should not be fetched and displayed`() =
        runTest {
            setAndResetLocale {
                var productRecommendationFetched = false
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = false,
                        ),
                        shoppingExperienceFeature = FakeShoppingExperienceFeature(
                            productRecommendationsExposureEnabled = false,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                            productRecommendation = {
                                productRecommendationFetched = true
                                ProductRecommendationTestData.productRecommendation()
                            },
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.waitUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productReviewState = ProductAnalysisTestData.analysisPresent(),
                    productRecommendationsPreference = false,
                    productRecommendationsExposure = false,
                    productVendor = ProductVendor.BEST_BUY,
                )
                assertEquals(expected, tested.state)
                assertFalse(productRecommendationFetched)
            }
        }

    @Test
    fun `GIVEN product recommendations are enabled WHEN a product analysis is fetched successfully and exposure is set to false THEN product recommendation should be fetched and displayed`() =
        runTest {
            setAndResetLocale {
                var productRecommendationFetched = false
                val tested = ReviewQualityCheckStore(
                    middleware = provideReviewQualityCheckMiddleware(
                        reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                            isEnabled = true,
                            isProductRecommendationsEnabled = true,
                        ),
                        shoppingExperienceFeature = FakeShoppingExperienceFeature(
                            productRecommendationsExposureEnabled = false,
                        ),
                        reviewQualityCheckService = FakeReviewQualityCheckService(
                            productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                            productRecommendation = {
                                productRecommendationFetched = true
                                ProductRecommendationTestData.productRecommendation()
                            },
                        ),
                    ),
                )
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()
                tested.waitUntilIdle()
                tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
                tested.waitUntilIdle()
                dispatcher.scheduler.advanceUntilIdle()

                val expected = ReviewQualityCheckState.OptedIn(
                    productReviewState = ProductAnalysisTestData.analysisPresent(
                        recommendedProductState = ProductRecommendationTestData.product(),
                    ),
                    productRecommendationsPreference = true,
                    productRecommendationsExposure = false,
                    productVendor = ProductVendor.BEST_BUY,
                )
                assertEquals(expected, tested.state)
                assertTrue(productRecommendationFetched)
            }
        }

    @Test
    fun `GIVEN product recommendations are enabled WHEN a product analysis is fetched successfully and product recommendation fails THEN product recommendations state should be initial`() =
        runTest {
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                        productRecommendation = { null },
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            val expected = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    recommendedProductState = ReviewQualityCheckState.RecommendedProductState.Initial,
                ),
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ProductVendor.BEST_BUY,
            )
            assertEquals(expected, tested.state)
        }

    @Test
    fun `GIVEN product recommendations are enabled WHEN product analysis fails THEN product recommendations should not be fetched`() =
        runTest {
            val captureActionsMiddleware =
                CaptureActionsMiddleware<ReviewQualityCheckState, ReviewQualityCheckAction>()
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { null },
                        productRecommendation = { ProductRecommendationTestData.productRecommendation() },
                    ),
                ) + captureActionsMiddleware,
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()

            captureActionsMiddleware.assertNotDispatched(ReviewQualityCheckAction.UpdateRecommendedProduct::class)
        }

    @Test
    fun `GIVEN product recommendations are enabled WHEN recommended product is clicked THEN click event is recorded`() =
        runTest {
            var productClicked: String? = null
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                        productRecommendation = {
                            ProductRecommendationTestData.productRecommendation(
                                aid = "342",
                            )
                        },
                        recordClick = {
                            productClicked = it
                        },
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(
                ReviewQualityCheckAction.RecommendedProductClick(
                    productAid = "342",
                    productUrl = "https://test.com",
                ),
            ).joinBlocking()

            assertEquals("342", productClicked)
        }

    @Test
    fun `GIVEN product recommendations are enabled WHEN recommended product is viewed THEN impression event is recorded`() =
        runTest {
            var productViewed: String? = null
            val tested = ReviewQualityCheckStore(
                middleware = provideReviewQualityCheckMiddleware(
                    reviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(
                        isEnabled = true,
                        isProductRecommendationsEnabled = true,
                    ),
                    reviewQualityCheckService = FakeReviewQualityCheckService(
                        productAnalysis = { ProductAnalysisTestData.productAnalysis() },
                        productRecommendation = {
                            ProductRecommendationTestData.productRecommendation(
                                aid = "342",
                            )
                        },
                        recordImpression = {
                            productViewed = it
                        },
                    ),
                ),
            )
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.waitUntilIdle()
            tested.dispatch(ReviewQualityCheckAction.FetchProductAnalysis).joinBlocking()
            tested.waitUntilIdle()
            dispatcher.scheduler.advanceUntilIdle()
            tested.dispatch(
                ReviewQualityCheckAction.RecommendedProductImpression(productAid = "342"),
            ).joinBlocking()

            assertEquals("342", productViewed)
        }

    private fun provideReviewQualityCheckMiddleware(
        reviewQualityCheckPreferences: ReviewQualityCheckPreferences = FakeReviewQualityCheckPreferences(),
        reviewQualityCheckVendorsService: FakeReviewQualityCheckVendorsService = FakeReviewQualityCheckVendorsService(),
        reviewQualityCheckService: ReviewQualityCheckService = FakeReviewQualityCheckService(),
        networkChecker: NetworkChecker = FakeNetworkChecker(),
        shoppingExperienceFeature: ShoppingExperienceFeature = FakeShoppingExperienceFeature(),
        appStore: AppStore = AppStore(),
    ): List<ReviewQualityCheckMiddleware> {
        return listOf(
            ReviewQualityCheckPreferencesMiddleware(
                reviewQualityCheckPreferences = reviewQualityCheckPreferences,
                reviewQualityCheckVendorsService = reviewQualityCheckVendorsService,
                appStore = appStore,
                shoppingExperienceFeature = shoppingExperienceFeature,
                scope = this.scope,
            ),
            ReviewQualityCheckNetworkMiddleware(
                reviewQualityCheckService = reviewQualityCheckService,
                networkChecker = networkChecker,
                scope = this.scope,
            ),
        )
    }

    private fun setAndResetLocale(locale: Locale = Locale.US, block: () -> Unit) {
        val initialLocale = Locale.getDefault()
        Locale.setDefault(locale)
        block()
        Locale.setDefault(initialLocale)
    }
}
