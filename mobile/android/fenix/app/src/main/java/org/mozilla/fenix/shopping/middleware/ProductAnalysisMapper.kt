/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.concept.engine.shopping.Highlight
import mozilla.components.concept.engine.shopping.ProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.HighlightsInfo

/**
 * Maps [ProductAnalysis] to [ProductReviewState].
 */
fun ProductAnalysis?.toProductReviewState(): ProductReviewState =
    this?.toProductReview() ?: ProductReviewState.Error.GenericError

private fun ProductAnalysis.toProductReview(): ProductReviewState =
    if (pageNotSupported) {
        ProductReviewState.Error.UnsupportedProductTypeError
    } else if (productId == null) {
        if (needsAnalysis) {
            ProductReviewState.NoAnalysisPresent()
        } else {
            ProductReviewState.Error.GenericError
        }
    } else if (notEnoughReviews && !needsAnalysis) {
        ProductReviewState.Error.NotEnoughReviews
    } else {
        val mappedRating = adjustedRating?.toFloat()
        val mappedGrade = grade?.asEnumOrDefault<ReviewQualityCheckState.Grade>()
        val mappedHighlights = highlights?.toHighlights()?.toSortedMap()

        if (mappedGrade == null && mappedRating == null && mappedHighlights == null) {
            ProductReviewState.NoAnalysisPresent()
        } else {
            ProductReviewState.AnalysisPresent(
                productId = productId!!,
                reviewGrade = mappedGrade,
                analysisStatus = needsAnalysis.toAnalysisStatus(),
                adjustedRating = mappedRating,
                productUrl = analysisURL!!,
                highlightsInfo = mappedHighlights?.let { HighlightsInfo(it) },
            )
        }
    }

private fun Boolean.toAnalysisStatus(): AnalysisStatus =
    when (this) {
        true -> AnalysisStatus.NEEDS_ANALYSIS
        false -> AnalysisStatus.UP_TO_DATE
    }

private fun Highlight.toHighlights(): Map<HighlightType, List<String>>? =
    HighlightType.values()
        .associateWith { highlightsForType(it) }
        .filterValues { it != null }
        .mapValues { it.value!! }
        .ifEmpty { null }

private fun Highlight.highlightsForType(highlightType: HighlightType) =
    when (highlightType) {
        HighlightType.QUALITY -> quality
        HighlightType.PRICE -> price
        HighlightType.SHIPPING -> shipping
        HighlightType.PACKAGING_AND_APPEARANCE -> appearance
        HighlightType.COMPETITIVENESS -> competitiveness
    }?.map { it.surroundWithQuotes() }

private fun String.surroundWithQuotes(): String =
    "\"$this\""
