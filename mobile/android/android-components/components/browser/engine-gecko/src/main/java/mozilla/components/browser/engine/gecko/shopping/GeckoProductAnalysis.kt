/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.shopping

import mozilla.components.concept.engine.shopping.ProductAnalysis

/**
 * Holds the result of the analysis of a shopping product.
 *
 * @property productId Product identifier (ASIN/SKU)
 * @property analysisURL Analysis URL
 * @property grade Reliability grade for the product's reviews
 * @property adjustedRating Product rating adjusted to exclude untrusted reviews
 * @property needsAnalysis Boolean indicating if the analysis is stale
 * @property lastAnalysisTime Time since the last analysis was performed
 * @property deletedProductReported Boolean indicating if reported that this product has been deleted
 * @property deletedProduct Boolean indicating if this product is now deleted
 * @property highlights Object containing highlights for product
 */
data class GeckoProductAnalysis(
    override val productId: String?,
    val analysisURL: String?,
    val grade: String?,
    val adjustedRating: Double,
    val needsAnalysis: Boolean,
    val lastAnalysisTime: Int,
    val deletedProductReported: Boolean,
    val deletedProduct: Boolean,
    val highlights: Highlight?,
) : ProductAnalysis

/**
 * Contains information about highlights of a product's reviews.
 *
 * @property quality Highlights about the quality of a product
 * @property price Highlights about the price of a product
 * @property shipping Highlights about the shipping of a product
 * @property appearance Highlights about the appearance of a product
 * @property competitiveness Highlights about the competitiveness of a product
 */
data class Highlight(
    val quality: List<String>?,
    val price: List<String>?,
    val shipping: List<String>?,
    val appearance: List<String>?,
    val competitiveness: List<String>?,
)
