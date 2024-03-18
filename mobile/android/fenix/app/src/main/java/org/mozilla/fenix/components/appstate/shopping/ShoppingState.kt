/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.shopping

/**
 * State for shopping feature that's required to live the lifetime of a session.
 *
 * @property shoppingSheetExpanded Boolean indicating if the shopping sheet is expanded and visible.
 * @property productCardState Map of product url to [CardState] that contains the state of different
 * cards in the shopping sheet.
 * @property recordedProductRecommendationImpressions Set of [ProductRecommendationImpressionKey]
 * that contains the product recommendation impressions that have been recorded.
 */
data class ShoppingState(
    val shoppingSheetExpanded: Boolean? = null,
    val productCardState: Map<String, CardState> = emptyMap(),
    val recordedProductRecommendationImpressions: Set<ProductRecommendationImpressionKey> = emptySet(),
) {

    /**
     * Key for a product recommendation impression.
     *
     * @property tabId The id of the tab that the product and recommendation is displayed in.
     * @property productUrl The url of the product.
     * @property aid The id of the recommendation.
     */
    data class ProductRecommendationImpressionKey(
        val tabId: String,
        val productUrl: String,
        val aid: String,
    )

    /**
     * State for different cards in the shopping sheet for a product.
     *
     * @property isHighlightsExpanded Whether or not the highlights card is expanded.
     * @property isInfoExpanded Whether or not the info card is expanded.
     * @property isSettingsExpanded Whether or not the settings card is expanded.
     */
    data class CardState(
        val isHighlightsExpanded: Boolean = false,
        val isInfoExpanded: Boolean = false,
        val isSettingsExpanded: Boolean = false,
    )
}
