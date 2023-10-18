/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.shopping

/**
 * Contains information about a product recommendation.
 *
 * @property url Url of recommended product.
 * @property analysisUrl Analysis URL.
 * @property adjustedRating Adjusted rating.
 * @property sponsored Whether or not it is a sponsored recommendation.
 * @property imageUrl Url of product recommendation image.
 * @property aid Unique identifier for the ad entity.
 * @property name Name of recommended product.
 * @property grade Grade of recommended product.
 * @property price Price of recommended product.
 * @property currency Currency of recommended product.
 */
data class ProductRecommendation(
    val url: String,
    val analysisUrl: String,
    val adjustedRating: Double,
    val sponsored: Boolean,
    val imageUrl: String,
    val aid: String,
    val name: String,
    val grade: String,
    val price: String,
    val currency: String,
)
