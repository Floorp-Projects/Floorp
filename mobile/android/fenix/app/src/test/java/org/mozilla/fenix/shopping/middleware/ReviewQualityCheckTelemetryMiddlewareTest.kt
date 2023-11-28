/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.store.BottomSheetDismissSource
import org.mozilla.fenix.shopping.store.BottomSheetViewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

@RunWith(FenixRobolectricTestRunner::class)
class ReviewQualityCheckTelemetryMiddlewareTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var store: ReviewQualityCheckStore
    private lateinit var browserStore: BrowserStore
    private lateinit var appStore: AppStore

    @Before
    fun setup() {
        browserStore = BrowserStore()
        appStore = AppStore()
        store = ReviewQualityCheckStore(
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        store.waitUntilIdle()
    }

    @Test
    fun `WHEN the user opts in the feature THEN the opt in event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OptIn).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceOptInAccepted.testGetValue())
    }

    @Test
    fun `WHEN the bottom sheet is closed THEN the bottom sheet closed event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.BottomSheetClosed(BottomSheetDismissSource.CLICK_OUTSIDE)).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceClosed.testGetValue())
        val event = Shopping.surfaceClosed.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals(BottomSheetDismissSource.CLICK_OUTSIDE.sourceName, event.single().extra?.getValue("source"))
    }

    @Test
    fun `WHEN the bottom sheet is displayed THEN the bottom sheet displayed event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.BottomSheetDisplayed(BottomSheetViewState.HALF_VIEW)).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceDisplayed.testGetValue())
        val event = Shopping.surfaceDisplayed.testGetValue()!!
        assertEquals(1, event.size)
        assertEquals(BottomSheetViewState.HALF_VIEW.state, event.single().extra?.getValue("view"))
    }

    @Test
    fun `WHEN the learn more link from the explainer card is clicked THEN the explainer learn more event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OpenExplainerLearnMoreLink).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceReviewQualityExplainerUrlClicked.testGetValue())
    }

    @Test
    fun `WHEN the terms and conditions link from the onboarding card is clicked THEN the terms and conditions event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OpenOnboardingTermsLink).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceShowTermsClicked.testGetValue())
    }

    @Test
    fun `WHEN the privacy policy link from the onboarding card is clicked THEN the privacy policy event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OpenOnboardingPrivacyPolicyLink).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceShowPrivacyPolicyClicked.testGetValue())
    }

    @Test
    fun `WHEN the learn more link from the onboarding card is clicked THEN the onboarding learn more event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OpenOnboardingLearnMoreLink).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceLearnMoreClicked.testGetValue())
    }

    @Test
    fun `WHEN the not now button from the onboarding card is clicked THEN the not now event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.NotNowClicked).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceNotNowClicked.testGetValue())
    }

    @Test
    fun `WHEN the expand button from the highlights card is clicked THEN the show more recent reviews event is recorded`() {
        val tested = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isHighlightsExpanded = false,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        tested.waitUntilIdle()
        tested.dispatch(ReviewQualityCheckAction.ExpandCollapseHighlights).joinBlocking()
        tested.waitUntilIdle()

        assertNotNull(Shopping.surfaceShowMoreRecentReviewsClicked.testGetValue())
    }

    @Test
    fun `WHEN the collapse button from the highlights card is clicked THEN the show more recent reviews event is not recorded`() {
        val tested = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isHighlightsExpanded = true,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        tested.waitUntilIdle()
        tested.dispatch(ReviewQualityCheckAction.ExpandCollapseHighlights).joinBlocking()
        tested.waitUntilIdle()

        assertNull(Shopping.surfaceShowMoreRecentReviewsClicked.testGetValue())
    }

    @Test
    fun `WHEN the expand button from the settings card is clicked THEN the settings expand event is recorded`() {
        val tested = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isSettingsExpanded = false,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        tested.waitUntilIdle()
        tested.dispatch(ReviewQualityCheckAction.ExpandCollapseSettings).joinBlocking()
        tested.waitUntilIdle()

        assertNotNull(Shopping.surfaceExpandSettings.testGetValue())
    }

    @Test
    fun `WHEN the collapse button from the settings card is clicked THEN the settings expand event is not recorded`() {
        val tested = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isSettingsExpanded = true,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        tested.waitUntilIdle()
        tested.dispatch(ReviewQualityCheckAction.ExpandCollapseSettings).joinBlocking()
        tested.waitUntilIdle()

        assertNull(Shopping.surfaceExpandSettings.testGetValue())
    }

    @Test
    fun `WHEN no analysis is present THEN the no analysis event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.NoAnalysisDisplayed).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceNoReviewReliabilityAvailable.testGetValue())
    }

    @Test
    fun `WHEN analyze button is clicked THEN the analyze reviews event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.AnalyzeProduct).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceAnalyzeReviewsNoneAvailableClicked.testGetValue())
    }

    @Test
    fun `WHEN reanalyze button is clicked THEN the reanalyze event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.ReanalyzeProduct).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceReanalyzeClicked.testGetValue())
    }

    @Test
    fun `WHEN back in stock button is clicked THEN the reactivate event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.ReportProductBackInStock).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceReactivatedButtonClicked.testGetValue())
    }

    @Test
    fun `WHEN the user is opted out after initializing the feature after THEN the onboarding displayed event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OptOutCompleted(emptyList())).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceOnboardingDisplayed.testGetValue())
    }

    @Test
    fun `WHEN the user is tapped the 'Powered by Fakespot by Mozilla' link THEN the link clicked telemetry is recorded`() {
        store.dispatch(ReviewQualityCheckAction.OpenPoweredByLink).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfacePoweredByFakespotLinkClicked.testGetValue())
    }

    @Test
    fun `GIVEN a product review has been updated WHEN stale analysis is present and product is not in analysis THEN the stale analysis event is recorded`() {
        val productTab = createTab(
            url = "pdp",
        )
        val browserState = BrowserState(
            tabs = listOf(productTab),
            selectedTabId = productTab.id,
        )
        val testedStore = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisPresent.AnalysisStatus.UP_TO_DATE,
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(BrowserStore(browserState), appStore),
            ),
        )
        val productReviewState = ProductAnalysisTestData.analysisPresent(
            analysisStatus = AnalysisPresent.AnalysisStatus.NEEDS_ANALYSIS,
        )

        testedStore.dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState)).joinBlocking()
        testedStore.waitUntilIdle()

        assertNotNull(Shopping.surfaceStaleAnalysisShown.testGetValue())
    }

    @Test
    fun `GIVEN a product review has been updated WHEN stale analysis is present and product is being analyzed THEN the stale analysis event is not recorded`() {
        val productTab = createTab(
            url = "pdp",
        )
        appStore.dispatch(AppAction.ShoppingAction.AddToProductAnalysed("pdp")).joinBlocking()
        appStore.waitUntilIdle()
        val browserState = BrowserState(
            tabs = listOf(productTab),
            selectedTabId = productTab.id,
        )
        val testedStore = ReviewQualityCheckStore(
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(BrowserStore(browserState), appStore),
            ),
        )
        val productReviewState = ProductAnalysisTestData.analysisPresent(
            analysisStatus = AnalysisPresent.AnalysisStatus.NEEDS_ANALYSIS,
        )

        testedStore.dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState)).joinBlocking()
        testedStore.waitUntilIdle()

        assertNull(Shopping.surfaceStaleAnalysisShown.testGetValue())
    }

    @Test
    fun `GIVEN a product review has been updated WHEN it is not a stale analysis THEN the stale analysis event is not recorded`() {
        val productTab = createTab(
            url = "pdp",
        )
        val browserState = BrowserState(
            tabs = listOf(productTab),
            selectedTabId = productTab.id,
        )
        val testedStore = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productReviewState = ProductAnalysisTestData.analysisPresent(
                    analysisStatus = AnalysisPresent.AnalysisStatus.NEEDS_ANALYSIS,
                ),
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(BrowserStore(browserState), appStore),
            ),
        )
        val productReviewState = ProductAnalysisTestData.analysisPresent(
            analysisStatus = AnalysisPresent.AnalysisStatus.REANALYZING,
        )

        testedStore.dispatch(ReviewQualityCheckAction.UpdateProductReview(productReviewState)).joinBlocking()
        testedStore.waitUntilIdle()

        assertNull(Shopping.surfaceStaleAnalysisShown.testGetValue())
    }

    @Test
    fun `WHEN a product recommendation is visible for more than one and a half seconds THEN ad impression telemetry probe is sent`() {
        store.dispatch(ReviewQualityCheckAction.RecommendedProductImpression("")).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceAdsImpression.testGetValue())
    }

    @Test
    fun `WHEN a product recommendation is clicked THEN the ad clicked telemetry probe is sent`() {
        store.dispatch(ReviewQualityCheckAction.RecommendedProductClick("", "")).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceAdsClicked.testGetValue())
    }

    @Test
    fun `GIVEN the user has opted in WHEN the user switches product recommendations on THEN send enabled product recommendations toggled telemetry probe`() {
        val tested = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = false,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isHighlightsExpanded = false,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        tested.waitUntilIdle()
        tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
        tested.waitUntilIdle()

        assertEquals("enabled", Shopping.surfaceAdsSettingToggled.testGetValue()!!.first().extra!!["action"])
    }

    @Test
    fun `GIVEN the user has opted in WHEN the user switches product recommendations off THEN send disabled product recommendations toggled telemetry probe`() {
        val tested = ReviewQualityCheckStore(
            initialState = ReviewQualityCheckState.OptedIn(
                productRecommendationsPreference = true,
                productRecommendationsExposure = true,
                productVendor = ReviewQualityCheckState.ProductVendor.AMAZON,
                isHighlightsExpanded = false,
            ),
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(browserStore, appStore),
            ),
        )
        tested.waitUntilIdle()
        tested.dispatch(ReviewQualityCheckAction.ToggleProductRecommendation).joinBlocking()
        tested.waitUntilIdle()

        assertEquals("disabled", Shopping.surfaceAdsSettingToggled.testGetValue()!!.first().extra!!["action"])
    }
}
