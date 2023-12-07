/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.concept.engine.shopping.Highlight
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.HighlightsInfo

class ProductAnalysisMapperTest {

    @Test
    fun `WHEN ProductAnalysis has data THEN it is mapped to AnalysisPresent`() {
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
            analysisStatus = AnalysisStatus.UpToDate,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
            highlightsInfo = HighlightsInfo(
                mapOf(
                    HighlightType.QUALITY to listOf("quality"),
                    HighlightType.PRICE to listOf("price"),
                    HighlightType.SHIPPING to listOf("shipping"),
                    HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                    HighlightType.COMPETITIVENESS to listOf("competitiveness"),
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN ProductAnalysis has data with some missing highlights THEN it is mapped to AnalysisPresent with the non null highlights`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            productId = "id1",
            grade = "C",
            needsAnalysis = true,
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
            analysisStatus = AnalysisStatus.NeedsAnalysis,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
            highlightsInfo = HighlightsInfo(
                mapOf(
                    HighlightType.QUALITY to listOf("quality"),
                    HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                    HighlightType.COMPETITIVENESS to listOf("competitiveness"),
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN ProductAnalysis has an invalid grade THEN it is mapped to AnalysisPresent with grade as null`() {
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
            analysisStatus = AnalysisStatus.UpToDate,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product analysis is null THEN it is mapped to Error`() {
        val actual = null.toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.GenericError

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product id is null and needs analysis is true THEN it is mapped to no analysis present`() {
        val actual =
            ProductAnalysisTestData.productAnalysis(
                productId = null,
                needsAnalysis = true,
            ).toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent()

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product id is null and needs analysis is false THEN it is mapped to no generic error`() {
        val actual =
            ProductAnalysisTestData.productAnalysis(
                productId = null,
                needsAnalysis = false,
            ).toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.GenericError

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN there are not enough reviews and no analysis needed THEN not enough reviews card is visible`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            notEnoughReviews = true,
            needsAnalysis = false,
        ).toProductReviewState()

        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.NotEnoughReviews

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN there are enough reviews and no analysis needed THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            notEnoughReviews = false,
            needsAnalysis = false,
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            productId = "1",
            reviewGrade = ReviewQualityCheckState.Grade.A,
            analysisStatus = AnalysisStatus.UpToDate,
            adjustedRating = 4.5f,
            productUrl = "https://test.com",
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN there are not enough reviews and analysis is needed THEN it is mapped to AnalysisPresent with NEEDS_ANALYSIS status`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            notEnoughReviews = true,
            needsAnalysis = true,
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            productId = "1",
            reviewGrade = ReviewQualityCheckState.Grade.A,
            analysisStatus = AnalysisStatus.NeedsAnalysis,
            adjustedRating = 4.5f,
            productUrl = "https://test.com",
            highlightsInfo = null,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN grade, rating and highlights are all null THEN it is mapped to no analysis present`() {
        val actual =
            ProductAnalysisTestData.productAnalysis(
                grade = null,
                adjustedRating = null,
                highlights = null,
            ).toProductReviewState()
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent()

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN only rating is available THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = null,
            adjustedRating = 3.5,
            highlights = null,
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = null,
            adjustedRating = 3.5f,
            highlightsInfo = null,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN only grade is available THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = "B",
            adjustedRating = null,
            highlights = null,
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = ReviewQualityCheckState.Grade.B,
            adjustedRating = null,
            highlightsInfo = null,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN only highlights are available THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = null,
            adjustedRating = null,
            highlights = Highlight(
                quality = listOf("quality"),
                price = null,
                shipping = null,
                appearance = listOf("appearance"),
                competitiveness = listOf("competitiveness"),
            ),
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = null,
            adjustedRating = null,
            highlightsInfo = HighlightsInfo(
                mapOf(
                    HighlightType.QUALITY to listOf("quality"),
                    HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                    HighlightType.COMPETITIVENESS to listOf("competitiveness"),
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN highlights and grade are available THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = "B",
            adjustedRating = null,
            highlights = Highlight(
                quality = listOf("quality"),
                price = null,
                shipping = null,
                appearance = listOf("appearance"),
                competitiveness = listOf("competitiveness"),
            ),
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = ReviewQualityCheckState.Grade.B,
            adjustedRating = null,
            highlightsInfo = HighlightsInfo(
                mapOf(
                    HighlightType.QUALITY to listOf("quality"),
                    HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                    HighlightType.COMPETITIVENESS to listOf("competitiveness"),
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN highlights and rating are available THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = null,
            adjustedRating = 3.4,
            highlights = Highlight(
                quality = listOf("quality"),
                price = null,
                shipping = null,
                appearance = listOf("appearance"),
                competitiveness = listOf("competitiveness"),
            ),
        ).toProductReviewState()

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = null,
            adjustedRating = 3.4f,
            highlightsInfo = HighlightsInfo(
                mapOf(
                    HighlightType.QUALITY to listOf("quality"),
                    HighlightType.PACKAGING_AND_APPEARANCE to listOf("appearance"),
                    HighlightType.COMPETITIVENESS to listOf("competitiveness"),
                ),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN page not supported is true THEN it is mapped to unsupported product error `() {
        val actual = ProductAnalysisTestData.productAnalysis(
            pageNotSupported = true,
        ).toProductReviewState()

        val expected =
            ReviewQualityCheckState.OptedIn.ProductReviewState.Error.UnsupportedProductTypeError

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product deleted is true and has not been reported THEN it is mapped to not available and not back in stock`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            deletedProduct = true,
            deletedProductReported = false,
        ).toProductReviewState()

        val expected =
            ReviewQualityCheckState.OptedIn.ProductReviewState.Error.ProductNotAvailable
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN product deleted is true and has been reported THEN it is mapped to not available and back in stock`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            deletedProduct = true,
            deletedProductReported = true,
        ).toProductReviewState()

        val expected =
            ReviewQualityCheckState.OptedIn.ProductReviewState.Error.ProductAlreadyReported
        assertEquals(expected, actual)
    }
}
