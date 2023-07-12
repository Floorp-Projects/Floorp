/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.state

import mozilla.components.lib.state.State

/**
 * UI state of the review quality check feature.
 */
sealed interface ReviewQualityCheckState : State {
    /**
     * The initial state of the feature, it's also the default state set in the store.
     */
    object Initial : ReviewQualityCheckState

    /**
     * The state when the user has not opted in for the feature.
     */
    object NotOptedIn : ReviewQualityCheckState

    /**
     * The state when the user has opted in for the feature.
     *
     * @property productReviewState The state of the product the user is browsing.
     * @property productRecommendationsPreference User preference whether to show product
     * recommendations. True if product recommendations should be shown.
     */
    data class OptedIn(
        val productReviewState: ProductReviewState = ProductReviewState.Loading,
        val productRecommendationsPreference: Boolean,
    ) : ReviewQualityCheckState {

        /**
         * The state of the product the user is browsing.
         */
        sealed interface ProductReviewState {
            /**
             * Denotes content is loading.
             */
            object Loading : ProductReviewState

            /**
             * Denotes an error has occurred.
             */
            object Error : ProductReviewState

            /**
             * Denotes no analysis is present for the product the user is browsing.
             *
             * @property productUrl The url of the product the user is browsing.
             */
            data class NoAnalysisPresent(
                val productUrl: String,
            ) : ProductReviewState

            /**
             * Denotes the state where analysis of the product is fetched and available.
             *
             * @property productId The id of the product, e.g ASIN, SKU.
             * @property reviewGrade The review grade of the product.
             * @property needsAnalysis If true, the analysis is stale and that to get the fresh
             * data, re–analysis is needed.
             * @property adjustedRating The adjusted rating taking review quality into consideration.
             * @property productUrl The url of the product the user is browsing.
             * @property highlights Optional highlights based on recent reviews of the product.
             * @property recommendedProductState The state of the recommended product.
             */
            data class ProductAnalysis(
                val productId: String,
                val reviewGrade: Grade,
                val needsAnalysis: Boolean,
                val adjustedRating: Float,
                val productUrl: String,
                val highlights: Map<HighlightType, List<String>>?,
                val recommendedProductState: RecommendedProductState = RecommendedProductState.Initial,
            ) : ProductReviewState
        }
    }

    /**
     * Review Grade of the product - A being the best and F being the worst. There is no grade E.
     */
    enum class Grade {
        A, B, C, D, F
    }

    /**
     * Factors for which highlights are available based on recent reviews of the product.
     */
    enum class HighlightType {
        QUALITY, PRICE, SHIPPING, PACKAGING_AND_APPEARANCE, COMPETITIVENESS
    }

    /**
     * The state of the recommended product.
     */
    sealed interface RecommendedProductState {
        /**
         * The initial state of the recommended product.
         */
        object Initial : RecommendedProductState

        /**
         * The state when the recommended product is loading.
         */
        object Loading : RecommendedProductState

        /**
         * The state when an error has occurred while fetching the recommended product.
         */
        object Error : RecommendedProductState

        /**
         * The state when the recommended product is available.
         *
         * @property name The name of the product.
         * @property productUrl The url of the product.
         * @property imageUrl The url of the image of the product.
         * @property formattedPrice The formatted price of the product.
         * @property reviewGrade The review grade of the product.
         * @property adjustedRating The adjusted rating of the product.
         * @property isSponsored True if the product is sponsored.
         * @property analysisUrl The url of the analysis of the product.
         */
        data class Product(
            val name: String,
            val productUrl: String,
            val imageUrl: String,
            val formattedPrice: String,
            val reviewGrade: Grade,
            val adjustedRating: Float,
            val isSponsored: Boolean,
            val analysisUrl: String,
        ) : RecommendedProductState
    }
}
