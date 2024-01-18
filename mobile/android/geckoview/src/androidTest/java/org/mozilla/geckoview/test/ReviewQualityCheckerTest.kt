/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertNull
import junit.framework.TestCase.assertTrue
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.* // ktlint-disable no-wildcard-imports
import org.mozilla.geckoview.GeckoSession.AnalysisStatusResponse
import org.mozilla.geckoview.GeckoSession.ContentDelegate
import org.mozilla.geckoview.GeckoSession.Recommendation
import org.mozilla.geckoview.GeckoSession.ReviewAnalysis
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@RunWith(AndroidJUnit4::class)
@MediumTest
class ReviewQualityCheckerTest : BaseSessionTest() {
    @Before
    fun setup() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "toolkit.shopping.ohttpRelayURL" to "",
                "toolkit.shopping.ohttpConfigURL" to "",
                "geckoview.shopping.mock_test_response" to true,
            ),
        )
    }

    @After
    fun cleanup() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "geckoview.shopping.mock_test_response" to false,
            ),
        )
    }

    @Test
    fun onProductUrl() {
        mainSession.loadUri("example.com")
        sessionRule.waitForPageStop()

        mainSession.loadUri("example.com/dp/ABCDEFG")
        sessionRule.waitForPageStop()

        // test below working product urls
        mainSession.loadUri("example.com/dp/ABCDEFG123")
        sessionRule.waitForPageStop()

        mainSession.loadUri("example.com/dp/HIJKLMN456")
        sessionRule.waitForPageStop()

        mainSession.loadUri("example.com/dp/OPQRSTU789")
        sessionRule.waitForPageStop()

        mainSession.delegateUntilTestEnd(object : ContentDelegate {
            @AssertCalled(count = 3)
            override fun onProductUrl(session: GeckoSession) {}
        })
    }

    @Test
    fun requestAnalysis() {
        // Test for the builder constructor
        val productId = "banana"
        val grade = "A"
        val adjustedRating = 4.5
        val lastAnalysisTime = 12345.toLong()
        val analysisURL = "https://analysis.com"

        val analysisObject = ReviewAnalysis.Builder(productId)
            .analysisUrl(analysisURL)
            .grade(grade)
            .adjustedRating(adjustedRating)
            .needsAnalysis(true)
            .pageNotSupported(false)
            .notEnoughReviews(false)
            .highlights(null)
            .lastAnalysisTime(lastAnalysisTime)
            .deletedProductReported(true)
            .deletedProduct(true)
            .build()
        assertThat("Analysis URL should match", analysisObject.analysisURL, equalTo(analysisURL))
        assertThat("Product id should match", analysisObject.productId, equalTo(productId))
        assertThat("Product grade should match", analysisObject.grade, equalTo(grade))
        assertThat("Product adjusted rating should match", analysisObject.adjustedRating, equalTo(adjustedRating))
        assertTrue("NeedsAnalysis should match", analysisObject.needsAnalysis)
        assertFalse("PageNotSupported should match", analysisObject.pageNotSupported)
        assertFalse("NotEnoughReviews should match", analysisObject.notEnoughReviews)
        assertNull("Highlights should match", analysisObject.highlights)
        assertTrue("Product should not be reported that it was deleted", analysisObject.deletedProductReported)
        assertTrue("Not a deleted product", analysisObject.deletedProduct)
        assertThat("Last analysis time should match", analysisObject.lastAnalysisTime, equalTo(lastAnalysisTime))

        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "geckoview.shopping.mock_test_response" to true,
            ),
        )
        val result = mainSession.requestAnalysis("https://www.example.com/mock")
        sessionRule.waitForResult(result).let {
            assertThat("Review analysis url should match", it.analysisURL, equalTo("https://www.example.com/mock_analysis_url"))
            assertThat("Product id should match", it.productId, equalTo("ABCDEFG123"))
            assertThat("Product grade should match", it.grade, equalTo("B"))
            assertThat("Product adjusted rating should match", it.adjustedRating, equalTo(4.5))
            assertTrue("NeedsAnalysis should match", it.needsAnalysis)
            assertTrue("PageNotSupported should match", it.pageNotSupported)
            assertTrue("NotEnoughReviews should match", it.notEnoughReviews)
            assertNull("Highlights should match", analysisObject.highlights)
            assertThat("Last analysis time should match", analysisObject.lastAnalysisTime, equalTo(lastAnalysisTime))
            assertTrue("DeletedProductReported should match", it.deletedProductReported)
            assertTrue("DeletedProduct should match", it.deletedProduct)
        }
    }

    @Test
    fun requestCreateAnalysisAndStatus() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "geckoview.shopping.mock_test_response" to true,
            ),
        )
        val createResult = mainSession.requestCreateAnalysis("https://www.example.com/mock/")
        assertThat("Analysis status should match", sessionRule.waitForResult(createResult), equalTo("pending"))

        val status = "in_progress"
        val progress = 90.9

        val analysisObject = AnalysisStatusResponse.Builder(status)
            .progress(progress)
            .build()
        assertThat("Analysis URL should match", analysisObject, notNullValue())
        assertThat("Analysis URL should match", analysisObject.status, equalTo(status))
        assertThat("Product id should match", analysisObject.progress, equalTo(progress))

        val statusResult = mainSession.requestAnalysisStatus("https://www.example.com/mock/")
        sessionRule.waitForResult(statusResult).let {
            assertThat(
                "Analysis status should match",
                it.status,
                equalTo("in_progress"),
            )
            assertThat(
                "Analysis progress should match",
                it.progress,
                equalTo(90.9),
            )
        }
    }

    @Test
    fun requestRecommendations() {
        // Test the Builder constructor
        val url = "https://example.com/mock_url"
        val adjustedRating = 3.5
        val imageUrl = "https://example.com/mock_image_url"
        val aid = "mock_aid"
        val name = "Mock Product"
        val grade = "C"
        val price = "450"
        val currency = "USD"

        val recommendationObject = Recommendation.Builder(url)
            .adjustedRating(adjustedRating)
            .sponsored(true)
            .imageUrl(imageUrl)
            .aid(aid)
            .name(name)
            .grade(grade)
            .price(price)
            .currency(currency)
            .build()
        assertThat("Recommendation URL should match", recommendationObject.url, equalTo(url))
        assertThat("Adjusted rating should match", recommendationObject.adjustedRating, equalTo(adjustedRating))
        assertThat("Recommendation sponsored field should match", recommendationObject.sponsored, equalTo(true))
        assertThat("Image URL should match", recommendationObject.imageUrl, equalTo(imageUrl))
        assertThat("Aid should match", recommendationObject.aid, equalTo(aid))
        assertThat("Name should match", recommendationObject.name, equalTo(name))
        assertThat("Grade should match", recommendationObject.grade, equalTo(grade))
        assertThat("Price should match", recommendationObject.price, equalTo(price))
        assertThat("Currency should match", recommendationObject.currency, equalTo(currency))

        val result = mainSession.requestRecommendations("https://www.example.com/mock")
        sessionRule.waitForResult(result)
            .let {
                assertThat("Recommendation URL should match", recommendationObject.url, equalTo(url))
                assertThat("Adjusted rating should match", recommendationObject.adjustedRating, equalTo(adjustedRating))
                assertThat("Recommendation sponsored field should match", recommendationObject.sponsored, equalTo(true))
                assertThat("Image URL should match", recommendationObject.imageUrl, equalTo(imageUrl))
                assertThat("Aid should match", recommendationObject.aid, equalTo(aid))
                assertThat("Name should match", recommendationObject.name, equalTo(name))
                assertThat("Grade should match", recommendationObject.grade, equalTo(grade))
                assertThat("Price should match", recommendationObject.price, equalTo(price))
                assertThat("Currency should match", recommendationObject.currency, equalTo(currency))
            }
    }

    @Test
    fun sendAttributionEvents() {
        val aid = "TEST_AID"
        val validClickResult = mainSession.sendClickAttributionEvent(aid)
        assertThat(
            "Click event success result should be true",
            sessionRule.waitForResult(validClickResult),
            equalTo(true),
        )
        val validImpressionResult = mainSession.sendImpressionAttributionEvent(aid)
        assertThat(
            "Impression event success result should be true",
            sessionRule.waitForResult(validImpressionResult),
            equalTo(true),
        )
        val validPlacementResult = mainSession.sendPlacementAttributionEvent(aid)
        assertThat(
            "Placement event success result should be true",
            sessionRule.waitForResult(validPlacementResult),
            equalTo(true),
        )
    }

    @Test
    fun reportBackInStock() {
        val result = mainSession.reportBackInStock("https://www.example.com/mock")
        assertThat("Report back in stock status matches", sessionRule.waitForResult(result), equalTo("report created"))
    }
}
