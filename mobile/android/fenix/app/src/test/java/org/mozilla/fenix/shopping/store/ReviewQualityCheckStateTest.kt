/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.HighlightsInfo
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.Progress

class ReviewQualityCheckStateTest {

    @Test
    fun `WHEN highlights are present THEN highlights to display in compact mode should contain first 2 highlights of the first highlight type`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                "High quality",
                "Excellent craftsmanship",
                "Superior materials",
            ),
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
                "Great value for money",
                "Discounted offers",
            ),
            ReviewQualityCheckState.HighlightType.SHIPPING to listOf(
                "Fast and reliable shipping",
                "Free shipping options",
                "Express delivery",
            ),
            ReviewQualityCheckState.HighlightType.PACKAGING_AND_APPEARANCE to listOf(
                "Elegant packaging",
                "Attractive appearance",
                "Beautiful design",
            ),
            ReviewQualityCheckState.HighlightType.COMPETITIVENESS to listOf(
                "Competitive pricing",
                "Strong market presence",
                "Unbeatable deals",
            ),
        )
        val highlightsInfo = HighlightsInfo(highlights)

        val expected = mapOf(
            ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                "High quality",
                "Excellent craftsmanship",
            ),
        )

        assertEquals(expected, highlightsInfo.highlightsForCompactMode)
    }

    @Test
    fun `WHEN only 1 highlight is present THEN highlights to display in compact mode should contain that one`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
            ),
        )
        val highlightsInfo = HighlightsInfo(highlights)

        val expected = mapOf(
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
            ),
        )

        assertEquals(expected, highlightsInfo.highlightsForCompactMode)
    }

    @Test
    fun `WHEN AnalysisPresent is created with grade, rating and highlights as null THEN exception is thrown`() {
        assertThrows(IllegalArgumentException::class.java) {
            ProductAnalysisTestData.analysisPresent(
                reviewGrade = null,
                adjustedRating = null,
                highlightsInfo = null,
            )
        }
    }

    @Test
    fun `WHEN AnalysisPresent is created with at least one of grade, rating and highlights as not null THEN no exception is thrown`() {
        val ratingPresent = kotlin.runCatching {
            ProductAnalysisTestData.analysisPresent(
                reviewGrade = null,
                adjustedRating = 1.2f,
                highlightsInfo = null,
            )
        }

        val gradePresent = kotlin.runCatching {
            ProductAnalysisTestData.analysisPresent(
                reviewGrade = ReviewQualityCheckState.Grade.A,
                adjustedRating = null,
                highlightsInfo = null,
            )
        }

        val highlightsPresent = kotlin.runCatching {
            ProductAnalysisTestData.analysisPresent(
                reviewGrade = null,
                adjustedRating = null,
                highlightsInfo = HighlightsInfo(
                    mapOf(ReviewQualityCheckState.HighlightType.QUALITY to listOf("")),
                ),
            )
        }

        assert(ratingPresent.isSuccess)
        assert(gradePresent.isSuccess)
        assert(highlightsPresent.isSuccess)
    }

    @Test
    fun `WHEN AnalysisPresent has more than 2 highlights snippets THEN show more button and highlights fade are visible`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                "High quality",
                "Excellent craftsmanship",
                "Superior materials",
            ),
            ReviewQualityCheckState.HighlightType.PRICE to listOf(
                "Affordable prices",
                "Great value for money",
                "Discounted offers",
            ),
            ReviewQualityCheckState.HighlightType.SHIPPING to listOf(
                "Fast and reliable shipping",
                "Free shipping options",
                "Express delivery",
            ),
            ReviewQualityCheckState.HighlightType.PACKAGING_AND_APPEARANCE to listOf(
                "Elegant packaging",
                "Attractive appearance",
                "Beautiful design",
            ),
            ReviewQualityCheckState.HighlightType.COMPETITIVENESS to listOf(
                "Competitive pricing",
                "Strong market presence",
                "Unbeatable deals",
            ),
        )
        val analysis = ProductAnalysisTestData.analysisPresent(
            highlightsInfo = HighlightsInfo(highlights),
        )

        assertTrue(analysis.highlightsInfo!!.highlightsFadeVisible)
        assertTrue(analysis.highlightsInfo!!.showMoreButtonVisible)
    }

    @Test
    fun `WHEN AnalysisPresent has exactly 1 highlights snippet THEN show more button and highlights fade are not visible`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.PRICE to listOf("Affordable prices"),
        )
        val analysis = ProductAnalysisTestData.analysisPresent(
            highlightsInfo = HighlightsInfo(highlights),
        )

        assertFalse(analysis.highlightsInfo!!.highlightsFadeVisible)
        assertFalse(analysis.highlightsInfo!!.showMoreButtonVisible)
    }

    @Test
    fun `WHEN AnalysisPresent has exactly 2 highlights snippets THEN show more button and highlights fade are not visible`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.SHIPPING to listOf(
                "Fast and reliable shipping",
                "Free shipping options",
            ),
        )
        val analysis = ProductAnalysisTestData.analysisPresent(
            highlightsInfo = HighlightsInfo(highlights),
        )

        assertFalse(analysis.highlightsInfo!!.highlightsFadeVisible)
        assertFalse(analysis.highlightsInfo!!.showMoreButtonVisible)
    }

    @Test
    fun `WHEN AnalysisPresent has a single highlights section and the section has more than 2 snippets THEN show more button and highlights fade are visible`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.SHIPPING to listOf(
                "Fast and reliable shipping",
                "Free shipping options",
                "Express delivery",
            ),
        )
        val analysis = ProductAnalysisTestData.analysisPresent(
            highlightsInfo = HighlightsInfo(highlights),
        )

        assertTrue(analysis.highlightsInfo!!.highlightsFadeVisible)
        assertTrue(analysis.highlightsInfo!!.showMoreButtonVisible)
    }

    @Test
    fun `WHEN AnalysisPresent has only 1 highlight snippet for the first category and more for others THEN show more button is visible and highlights fade is not visible`() {
        val highlights = mapOf(
            ReviewQualityCheckState.HighlightType.QUALITY to listOf(
                "High quality",
            ),
            ReviewQualityCheckState.HighlightType.PACKAGING_AND_APPEARANCE to listOf(
                "Elegant packaging",
                "Attractive appearance",
                "Beautiful design",
            ),
            ReviewQualityCheckState.HighlightType.COMPETITIVENESS to listOf(
                "Competitive pricing",
                "Strong market presence",
                "Unbeatable deals",
            ),
        )
        val analysis = ProductAnalysisTestData.analysisPresent(
            highlightsInfo = HighlightsInfo(highlights),
        )

        assertTrue(analysis.highlightsInfo!!.showMoreButtonVisible)
        assertFalse(analysis.highlightsInfo!!.highlightsFadeVisible)
    }

    @Test
    fun `WHEN progress has a positive value THEN normalized progress should match`() {
        val progress = Progress(61.6f)
        assertEquals(0.616f, progress.normalizedProgress)
    }

    @Test
    fun `WHEN no analysis is present with progress THEN normalized progress should match and progress bar is visible`() {
        val analysis = ProductAnalysisTestData.noAnalysisPresent(progress = 61.6f)
        assertTrue(analysis.isProgressBarVisible)
        assertEquals(0.616f, analysis.progress.normalizedProgress)
    }

    @Test
    fun `WHEN no analysis is present with negative progress THEN progress bar is not visible`() {
        val analysis = ProductAnalysisTestData.noAnalysisPresent(progress = -1f)
        assertFalse(analysis.isProgressBarVisible)
    }
}
