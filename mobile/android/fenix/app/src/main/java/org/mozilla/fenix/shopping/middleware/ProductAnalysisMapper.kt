/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.engine.gecko.shopping.GeckoProductAnalysis
import mozilla.components.browser.engine.gecko.shopping.Highlight
import mozilla.components.concept.engine.shopping.ProductAnalysis
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus

/**
 * Maps [ProductAnalysis] to [ProductReviewState].
 */
fun GeckoProductAnalysis?.toProductReviewState(isInitialAnalysis: Boolean = true): ProductReviewState =
    this?.toProductReview(isInitialAnalysis) ?: ProductReviewState.Error.GenericError

private fun GeckoProductAnalysis.toProductReview(isInitialAnalysis: Boolean): ProductReviewState =
    if (pageNotSupported) {
        ProductReviewState.Error.UnsupportedProductTypeError
    } else if (productId == null) {
        if (needsAnalysis) {
            ProductReviewState.NoAnalysisPresent()
        } else {
            ProductReviewState.Error.GenericError
        }
    } else {
        val mappedRating = adjustedRating?.toFloat()
        val mappedGrade = grade?.toGrade()
        val mappedHighlights = highlights?.toHighlights()?.toSortedMap()

        if (mappedGrade == null && mappedRating == null && mappedHighlights == null) {
            if (isInitialAnalysis) {
                ProductReviewState.NoAnalysisPresent()
            } else {
                ProductReviewState.Error.NotEnoughReviews
            }
        } else {
            ProductReviewState.AnalysisPresent(
                productId = productId!!,
                reviewGrade = mappedGrade,
                analysisStatus = needsAnalysis.toAnalysisStatus(),
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
