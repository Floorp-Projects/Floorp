/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import mozilla.components.lib.state.Action
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.RecommendedProductState

/**
 * Actions for review quality check feature.
 */
sealed interface ReviewQualityCheckAction : Action {

    /**
     * Actions that cause updates to state.
     */
    sealed interface UpdateAction : ReviewQualityCheckAction

    /**
     * Actions related to preferences.
     */
    sealed interface PreferencesMiddlewareAction : ReviewQualityCheckAction

    /**
     * Actions related to navigation events.
     */
    sealed interface NavigationMiddlewareAction : ReviewQualityCheckAction

    /**
     * Actions related to network requests.
     */
    sealed interface NetworkAction : ReviewQualityCheckAction

    /**
     * Actions related to telemetry events.
     */
    sealed interface TelemetryAction : ReviewQualityCheckAction

    /**
     * Triggered when the store is initialized.
     */
    object Init : PreferencesMiddlewareAction

    /**
     * Triggered when the user has opted in to the review quality check feature.
     */
    object OptIn : PreferencesMiddlewareAction, TelemetryAction

    /**
     * Triggered when the user has opted out of the review quality check feature.
     */
    object OptOut : PreferencesMiddlewareAction, UpdateAction, TelemetryAction

    /**
     * Triggered when the user has enabled or disabled product recommendations.
     */
    object ToggleProductRecommendation : PreferencesMiddlewareAction, UpdateAction, TelemetryAction

    /**
     * Triggered as a result of a [OptIn] or [Init] whe user has opted in for shopping experience.
     *
     * @property isProductRecommendationsEnabled Reflects the user preference update to display
     * recommended product. Null when product recommendations feature is disabled.
     * @property productRecommendationsExposure Whether product recommendations exposure is enabled.
     * @property productVendor The vendor of the product.
     * @property isHighlightsExpanded Whether the highlights card should be expanded.
     * @property isInfoExpanded Whether the info card should be expanded.
     * @property isSettingsExpanded Whether the settings card should be expanded.
     */
    data class OptInCompleted(
        val isProductRecommendationsEnabled: Boolean?,
        val productRecommendationsExposure: Boolean,
        val productVendor: ReviewQualityCheckState.ProductVendor,
        val isHighlightsExpanded: Boolean,
        val isInfoExpanded: Boolean,
        val isSettingsExpanded: Boolean,
    ) : UpdateAction

    /**
     * Triggered as a result of [Init] when user has opted out of shopping experience.
     *
     * @property productVendors List of product vendors in relevant order.
     */
    data class OptOutCompleted(
        val productVendors: List<ReviewQualityCheckState.ProductVendor>,
    ) : UpdateAction, TelemetryAction

    /**
     * Triggered as a result of a [NetworkAction] to update the [ProductReviewState].
     *
     * @property productReviewState The product review state to update.
     * @property restoreAnalysis Signals whether the analysis will be restored right after the update.
     */
    data class UpdateProductReview(
        val productReviewState: ProductReviewState,
        val restoreAnalysis: Boolean,
    ) : UpdateAction, TelemetryAction

    /**
     * Triggered as a result of a [NetworkAction] to update the [RecommendedProductState].
     */
    data class UpdateRecommendedProduct(
        val recommendedProductState: RecommendedProductState,
    ) : UpdateAction

    /**
     * Triggered when the user has opted in to the review quality check feature and the UI is opened.
     */
    object FetchProductAnalysis : NetworkAction, UpdateAction

    /**
     * Triggered when the user retries to fetch product analysis after a failure.
     */
    object RetryProductAnalysis : NetworkAction, UpdateAction

    /**
     * Triggered when the user triggers product re-analysis.
     */
    object ReanalyzeProduct : NetworkAction, UpdateAction, TelemetryAction

    /**
     * Triggered when the product was previously known to be in reanalysis
     * process when the sheet was closed and the state should be restored.
     */
    object RestoreReanalysis : NetworkAction, UpdateAction

    /**
     * Triggered when the user clicks on the analyze button
     */
    object AnalyzeProduct : NetworkAction, UpdateAction, TelemetryAction

    /**
     * Triggered when the user clicks on the recommended product.
     *
     * @property productAid The product's aid.
     * @property productUrl The product's link to open.
     */
    data class RecommendedProductClick(
        val productAid: String,
        val productUrl: String,
    ) : NavigationMiddlewareAction, NetworkAction, TelemetryAction

    /**
     * Triggered when the user views the recommended product.
     *
     * @property productAid The product's aid.
     */
    data class RecommendedProductImpression(
        val productAid: String,
    ) : NetworkAction, TelemetryAction

    /**
     * Triggered when the user clicks on learn more link on the explainer card.
     */
    object OpenExplainerLearnMoreLink : NavigationMiddlewareAction, TelemetryAction

    /**
     * Triggered when the user clicks on the "Powered by" link in the footer.
     */
    object OpenPoweredByLink : NavigationMiddlewareAction, TelemetryAction

    /**
     * Triggered when the user clicks on learn more link on the opt in card.
     */
    object OpenOnboardingLearnMoreLink : NavigationMiddlewareAction, TelemetryAction

    /**
     * Triggered when the user clicks on terms and conditions link on the opt in card.
     */
    object OpenOnboardingTermsLink : NavigationMiddlewareAction, TelemetryAction

    /**
     * Triggered when the user clicks on privacy policy link on the opt in card.
     */
    object OpenOnboardingPrivacyPolicyLink : NavigationMiddlewareAction, TelemetryAction

    /**
     * Triggered when the bottom sheet is closed.
     *
     * @property source The source of dismissal.
     */
    data class BottomSheetClosed(val source: BottomSheetDismissSource) : TelemetryAction

    /**
     * Triggered when the bottom sheet is opened.
     *
     * @property view The state of the bottom sheet when opened.
     */
    data class BottomSheetDisplayed(val view: BottomSheetViewState) : TelemetryAction

    /**
     * Triggered when the user clicks on the "Not now" button from the contextual onboarding card.
     */
    object NotNowClicked : TelemetryAction

    /**
     * Triggered when the user expands the recent reviews card.
     */
    object ExpandCollapseHighlights : TelemetryAction, UpdateAction, PreferencesMiddlewareAction

    /**
     * Triggered when the user expands or collapses the settings card.
     */
    object ExpandCollapseSettings : TelemetryAction, UpdateAction, PreferencesMiddlewareAction

    /**
     * Triggered when the user expands or collapses the info card.
     */
    object ExpandCollapseInfo : UpdateAction, PreferencesMiddlewareAction

    /**
     * Triggered when the No analysis card is displayed to the user.
     */
    object NoAnalysisDisplayed : TelemetryAction

    /**
     * Triggered when the user reports a product is back in stock.
     */
    object ReportProductBackInStock : TelemetryAction
}
