/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.shopping.store.BottomSheetDismissSource
import org.mozilla.fenix.shopping.store.BottomSheetViewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckAction
import org.mozilla.fenix.shopping.store.ReviewQualityCheckStore

@RunWith(FenixRobolectricTestRunner::class)
class ReviewQualityCheckTelemetryMiddlewareTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var store: ReviewQualityCheckStore

    @Before
    fun setup() {
        store = ReviewQualityCheckStore(
            middleware = listOf(
                ReviewQualityCheckTelemetryMiddleware(),
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
        Assert.assertEquals(1, event.size)
        Assert.assertEquals(BottomSheetDismissSource.CLICK_OUTSIDE.sourceName, event.single().extra?.getValue("source"))
    }

    @Test
    fun `WHEN the bottom sheet is displayed THEN the bottom sheet displayed event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.BottomSheetDisplayed(BottomSheetViewState.HALF_VIEW)).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceDisplayed.testGetValue())
        val event = Shopping.surfaceDisplayed.testGetValue()!!
        Assert.assertEquals(1, event.size)
        Assert.assertEquals(BottomSheetViewState.HALF_VIEW.state, event.single().extra?.getValue("view"))
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
        store.dispatch(ReviewQualityCheckAction.ShowMoreRecentReviewsClicked).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceShowMoreRecentReviewsClicked.testGetValue())
    }

    @Test
    fun `WHEN the expand button from the settings card is clicked THEN the settings expand event is recorded`() {
        store.dispatch(ReviewQualityCheckAction.ExpandSettingsClicked).joinBlocking()
        store.waitUntilIdle()

        assertNotNull(Shopping.surfaceExpandSettings.testGetValue())
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
}
