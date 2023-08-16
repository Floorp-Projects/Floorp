/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import mozilla.components.browser.engine.gecko.shopping.GeckoProductAnalysis
import mozilla.components.browser.engine.gecko.shopping.Highlight
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import java.util.SortedMap

object ProductAnalysisTestData {

    fun productAnalysis(
        productId: String? = "1",
        analysisURL: String = "https://test.com",
        grade: String? = "A",
        adjustedRating: Double = 4.5,
        needsAnalysis: Boolean = false,
        lastAnalysisTime: Long = 0L,
        deletedProductReported: Boolean = false,
        deletedProduct: Boolean = false,
        highlights: Highlight? = null,
    ): GeckoProductAnalysis = GeckoProductAnalysis(
        productId = productId,
        analysisURL = analysisURL,
        grade = grade,
        adjustedRating = adjustedRating,
        needsAnalysis = needsAnalysis,
        lastAnalysisTime = lastAnalysisTime,
        deletedProductReported = deletedProductReported,
        deletedProduct = deletedProduct,
        highlights = highlights,
    )

    fun analysisPresent(
        productId: String = "1",
        productUrl: String = "https://test.com",
        reviewGrade: ReviewQualityCheckState.Grade? = ReviewQualityCheckState.Grade.A,
        adjustedRating: Float? = 4.5f,
        needsAnalysis: Boolean = false,
        highlights: SortedMap<ReviewQualityCheckState.HighlightType, List<String>>? = null,
    ): ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent =
        ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent(
            productId = productId,
            productUrl = productUrl,
            reviewGrade = reviewGrade,
            adjustedRating = adjustedRating,
            needsAnalysis = needsAnalysis,
            highlights = highlights,
        )
}
