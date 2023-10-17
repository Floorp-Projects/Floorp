/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import mozilla.components.browser.engine.gecko.shopping.Highlight
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.shopping.ProductAnalysisTestData
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.HighlightType
import org.mozilla.fenix.shopping.store.ReviewQualityCheckState.OptedIn.ProductReviewState.AnalysisPresent.AnalysisStatus

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
            analysisStatus = AnalysisStatus.UP_TO_DATE,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("\"quality\""),
                HighlightType.PRICE to listOf("\"price\""),
                HighlightType.SHIPPING to listOf("\"shipping\""),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("\"appearance\""),
                HighlightType.COMPETITIVENESS to listOf("\"competitiveness\""),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN GeckoProductAnalysis has data with some missing highlights THEN it is mapped to AnalysisPresent with the non null highlights`() {
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
            analysisStatus = AnalysisStatus.NEEDS_ANALYSIS,
            adjustedRating = 3.4f,
            productUrl = "https://example.com",
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("\"quality\""),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("\"appearance\""),
                HighlightType.COMPETITIVENESS to listOf("\"competitiveness\""),
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
            analysisStatus = AnalysisStatus.UP_TO_DATE,
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
    fun `WHEN grade, rating and highlights are all null and it is initial analysis THEN it is mapped to no analysis present`() {
        val actual =
            ProductAnalysisTestData.productAnalysis(
                grade = null,
                adjustedRating = null,
                highlights = null,
            ).toProductReviewState(isInitialAnalysis = true)
        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.NoAnalysisPresent()

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN grade, rating and highlights are all null and it is not initial analysis THEN not enough reviews card is visible`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = null,
            adjustedRating = null,
            highlights = null,
        ).toProductReviewState(isInitialAnalysis = false)

        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.NotEnoughReviews

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN only rating is available it is not initial analysis THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = null,
            adjustedRating = 3.5,
            highlights = null,
        ).toProductReviewState(isInitialAnalysis = false)

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = null,
            adjustedRating = 3.5f,
            highlights = null,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN only grade is available it is not initial analysis THEN it is mapped to AnalysisPresent`() {
        val actual = ProductAnalysisTestData.productAnalysis(
            grade = "B",
            adjustedRating = null,
            highlights = null,
        ).toProductReviewState(isInitialAnalysis = false)

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = ReviewQualityCheckState.Grade.B,
            adjustedRating = null,
            highlights = null,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN only highlights are available it is not initial analysis THEN it is mapped to AnalysisPresent`() {
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
        ).toProductReviewState(isInitialAnalysis = false)

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = null,
            adjustedRating = null,
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("\"quality\""),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("\"appearance\""),
                HighlightType.COMPETITIVENESS to listOf("\"competitiveness\""),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN highlights and grade are available it is not initial analysis THEN it is mapped to AnalysisPresent`() {
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
        ).toProductReviewState(isInitialAnalysis = false)

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = ReviewQualityCheckState.Grade.B,
            adjustedRating = null,
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("\"quality\""),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("\"appearance\""),
                HighlightType.COMPETITIVENESS to listOf("\"competitiveness\""),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN highlights and rating are available it is not initial analysis THEN it is mapped to AnalysisPresent`() {
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
        ).toProductReviewState(isInitialAnalysis = false)

        val expected = ProductAnalysisTestData.analysisPresent(
            reviewGrade = null,
            adjustedRating = 3.4f,
            highlights = sortedMapOf(
                HighlightType.QUALITY to listOf("\"quality\""),
                HighlightType.PACKAGING_AND_APPEARANCE to listOf("\"appearance\""),
                HighlightType.COMPETITIVENESS to listOf("\"competitiveness\""),
            ),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN page not supported is true THEN it is mapped to unsupported product error `() {
        val actual = ProductAnalysisTestData.productAnalysis(
            pageNotSupported = true,
        ).toProductReviewState()

        val expected = ReviewQualityCheckState.OptedIn.ProductReviewState.Error.UnsupportedProductTypeError

        assertEquals(expected, actual)
    }
}
