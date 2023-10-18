/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping

import mozilla.components.concept.engine.shopping.Highlight
import mozilla.components.concept.engine.shopping.ProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus
import java.util.SortedMap

object ProductAnalysisTestData {

    fun productAnalysis(
        productId: String? = "1",
        analysisURL: String = "https://test.com",
        grade: String? = "A",
        adjustedRating: Double? = 4.5,
        needsAnalysis: Boolean = false,
        pageNotSupported: Boolean = false,
        lastAnalysisTime: Long = 0L,
        deletedProductReported: Boolean = false,
        deletedProduct: Boolean = false,
        highlights: Highlight? = null,
    ): ProductAnalysis = ProductAnalysis(
        productId = productId,
        analysisURL = analysisURL,
        grade = grade,
        adjustedRating = adjustedRating,
        needsAnalysis = needsAnalysis,
        pageNotSupported = pageNotSupported,
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
        analysisStatus: AnalysisStatus = AnalysisStatus.UP_TO_DATE,
        highlights: SortedMap<ReviewQualityCheckState.HighlightType, List<String>>? = null,
    ): ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent =
        ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent(
            productId = productId,
            productUrl = productUrl,
            reviewGrade = reviewGrade,
            adjustedRating = adjustedRating,
            analysisStatus = analysisStatus,
            highlights = highlights,
        )
}
