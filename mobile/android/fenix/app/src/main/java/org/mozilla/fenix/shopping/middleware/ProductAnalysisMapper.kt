/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.engine.gecko.shopping.GeckoProductAnalysis
import mozilla.components.browser.engine.gecko.shopping.Highlight
import mozilla.components.concept.engine.shopping.ProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState

/**
 * Maps [ProductAnalysis] to [ProductReviewState].
 */
fun ProductAnalysis?.toProductReviewState(): ProductReviewState =
    if (this == null) {
        ProductReviewState.Error
    } else {
        when (this) {
            is GeckoProductAnalysis -> toProductReview()
            else -> ProductReviewState.Error
        }
    }

private fun GeckoProductAnalysis.toProductReview(): ProductReviewState =
    if (productId == null) {
        ProductReviewState.Error
    } else {
        val mappedRating = adjustedRating.toFloatOrNull()
        val mappedGrade = grade?.toGrade()
        val mappedHighlights = highlights?.toHighlights()?.toSortedMap()

        if (mappedGrade == null && mappedRating == null && mappedHighlights == null) {
            ProductReviewState.NoAnalysisPresent
        } else {
            ProductReviewState.AnalysisPresent(
                productId = productId!!,
                reviewGrade = mappedGrade,
                needsAnalysis = needsAnalysis,
                adjustedRating = mappedRating,
                productUrl = analysisURL!!,
                highlights = mappedHighlights,
            )
        }
    }

private fun String.toGrade(): ReviewQualityCheckState.Grade? =
    try {
        ReviewQualityCheckState.Grade.valueOf(this)
    } catch (e: IllegalArgumentException) {
        null
    }

private fun Highlight.toHighlights(): Map<ReviewQualityCheckState.HighlightType, List<String>>? {
    val highlights: Map<ReviewQualityCheckState.HighlightType, List<String>?> = mapOf(
        ReviewQualityCheckState.HighlightType.QUALITY to quality,
        ReviewQualityCheckState.HighlightType.PRICE to price,
        ReviewQualityCheckState.HighlightType.SHIPPING to shipping,
        ReviewQualityCheckState.HighlightType.PACKAGING_AND_APPEARANCE to appearance,
        ReviewQualityCheckState.HighlightType.COMPETITIVENESS to competitiveness,
    )

    return highlights.filterValues { it != null }.mapValues { it.value!! }.ifEmpty { null }
}

private fun Double.toFloatOrNull(): Float? =
    if (this == 0.0) {
        null
    } else {
        toFloat()
    }
