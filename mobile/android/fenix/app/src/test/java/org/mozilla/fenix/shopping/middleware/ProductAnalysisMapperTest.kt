/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.engine.gecko.shopping.Highlight
import mozilla.components.concept.engine.shopping.ProductAnalysis
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType

class ProductAnalysisMapperTest {

    @Test
    fun `WHEN GeckoProductAnalysis has data THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            productId = "id1",
            grade = "C",
            needsAnalysis = false,
            adjustedRating = 3.4,
            analysisURL = "https://example.com",
            highlights = Highlight(
                quality = listOf("quality"),
                price = listOf("price"),
                shipping = listOf("shipping"),
                appearance = listOf("appearance"),
                competitiveness = listOf("competitiveness"),
            ),
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            productId = "id1",
            reviewGrade = ReviewQualityCheckState.Grade.C,
            needsAnalysis = false,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("quality"),
                HighlightType.PRICE to listOf("price"),
                HighlightType.SHIPPING to listOf("shipping"),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                HighlightType.COMPETITIVENESS to listOf("competitiveness"),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN GeckoProductAnalysis has data with some missing highlights THEN it is mapped to AnalysisPresent with the non null highlights`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            productId = "id1",
            grade = "C",
            needsAnalysis = false,
            adjustedRating = 3.4,
            analysisURL = "https://example.com",
            highlights = Highlight(
                quality = listOf("quality"),
                price = null,
                shipping = null,
                appearance = listOf("appearance"),
                competitiveness = listOf("competitiveness"),
            ),
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            productId = "id1",
            reviewGrade = ReviewQualityCheckState.Grade.C,
            needsAnalysis = false,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("quality"),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                HighlightType.COMPETITIVENESS to listOf("competitiveness"),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN GeckoProductAnalysis has an invalid grade THEN it is mapped to AnalysisPresent with grade as null`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            productId = "id1",
            grade = "?",
            needsAnalysis = false,
            adjustedRating = 3.4,
            analysisURL = "https://example.com",
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            productId = "id1",
            reviewGrade = null,
            needsAnalysis = false,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product analysis is null THEN it is mapped to Error`() {
        val actual = null.toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product id is null THEN it is mapped to Error`() {
        val actual =
            ProductAnalysisTestData.productAnalysis(productId = null).toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN grade, rating and highlights are all null THEN it is mapped to no analysis present`() {
        val actual =
            ProductAnalysisTestData.productAnalysis(
                grade = null,
                adjustedRating = 0.0,
                highlights = null,
            ).toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN ProductAnalysis is not GeckoProductAnalysis THEN it is mapped to Error`() {
        val randomAnalysis = object : ProductAnalysis {
            override val productId: String = "id1"
        }

        val actual = randomAnalysis.toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error

        assertEquals(expected, actual)
    }
}
