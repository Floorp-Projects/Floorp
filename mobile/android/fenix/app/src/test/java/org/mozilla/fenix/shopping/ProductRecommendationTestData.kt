/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import mozilla.components.concept.engine.shopping.ProductRecommendation
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState

object ProductRecommendationTestData {

    fun productRecommendation(
        aid: String = "aid",
        url: String = "https://test.com",
        grade: String = "A",
        adjustedRating: Double = 4.7,
        sponsored: Boolean = true,
        analysisUrl: String = "analysisUrl",
        imageUrl: String = "https://imageurl.com",
        name: String = "Test Product",
        price: String = "100",
        currency: String = "USD",
    ): ProductRecommendation = ProductRecommendation(
        aid = aid,
        url = url,
        grade = grade,
        adjustedRating = adjustedRating,
        sponsored = sponsored,
        analysisUrl = analysisUrl,
        imageUrl = imageUrl,
        name = name,
        price = price,
        currency = currency,
    )

    fun product(
        aid: String = "aid",
        url: String = "https://test.com",
        reviewGrade: ReviewQualityCheckState.Grade = ReviewQualityCheckState.Grade.A,
        adjustedRating: Double = 4.7,
        sponsored: Boolean = true,
        analysisUrl: String = "analysisUrl",
        imageUrl: String = "https://imageurl.com",
        name: String = "Test Product",
        formattedPrice: String = "$100",
    ): ReviewQualityCheckState.RecommendedProductState.Product =
        ReviewQualityCheckState.RecommendedProductState.Product(
            aid = aid,
            productUrl = url,
            reviewGrade = reviewGrade,
            adjustedRating = adjustedRating.toFloat(),
            isSponsored = sponsored,
            analysisUrl = analysisUrl,
            imageUrl = imageUrl,
            name = name,
            formattedPrice = formattedPrice,
        )
}
