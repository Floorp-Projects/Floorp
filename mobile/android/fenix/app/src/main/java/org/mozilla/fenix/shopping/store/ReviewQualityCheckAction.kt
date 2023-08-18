/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import mozilla.components.lib.state.Action
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState

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
     * Triggered when the store is initialized.
     */
    object Init : PreferencesMiddlewareAction

    /**
     * Triggered when the user has opted in to the review quality check feature.
     */
    object OptIn : PreferencesMiddlewareAction

    /**
     * Triggered when the user has opted out of the review quality check feature.
     */
    object OptOut : PreferencesMiddlewareAction, UpdateAction

    /**
     * Triggered when the user has enabled or disabled product recommendations.
     */
    object ToggleProductRecommendation : PreferencesMiddlewareAction, UpdateAction

    /**
     * Triggered as a result of a [PreferencesMiddlewareAction] to update the state.
     */
    data class UpdateUserPreferences(
        val hasUserOptedIn: Boolean,
        val isProductRecommendationsEnabled: Boolean,
    ) : UpdateAction

    /**
     * Triggered as a result of a [NetworkAction] to update the state.
     */
    data class UpdateProductReview(val productReviewState: ProductReviewState) : UpdateAction

    /**
     * Triggered when the user has opted in to the review quality check feature and the UI is opened.
     */
    object FetchProductAnalysis : NetworkAction

    /**
     * Triggered when the user retries to fetch product analysis after a failure.
     */
    object RetryProductAnalysis : NetworkAction

    /**
     * Triggered when opening a link from the review quality check feature.
     */
    data class OpenLink(val link: ReviewQualityCheckState.LinkType) : NavigationMiddlewareAction
}
