/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import com.google.android.play.core.review.ReviewManager
import com.google.android.play.core.review.ReviewManagerFactory
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.ReviewPrompt
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class TestReviewSettings(
    override var numberOfAppLaunches: Int = 0,
    var isDefault: Boolean = false,
    override var lastReviewPromptTimeInMillis: Long = 0,
) : ReviewSettings {
    override val isDefaultBrowser: Boolean
        get() = isDefault
}

@RunWith(FenixRobolectricTestRunner::class)
class ReviewPromptControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var reviewManager: ReviewManager

    @Before
    fun setUp() {
        reviewManager = ReviewManagerFactory.create(testContext)
    }

    @Test
    fun promptReviewDoesNotSetMillis() = runTest {
        var promptWasCalled = false
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = false,
            lastReviewPromptTimeInMillis = 0L,
        )

        val controller = ReviewPromptController(
            reviewManager,
            settings,
            { 100L },
            { promptWasCalled = true },
        )

        controller.reviewPromptIsReady = true
        controller.promptReview(HomeActivity())

        assertEquals(settings.lastReviewPromptTimeInMillis, 0L)
        assertFalse(promptWasCalled)
    }

    @Test
    fun promptReviewSetsMillisIfSuccessful() = runTest {
        var promptWasCalled = false
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L,
        )

        val controller = ReviewPromptController(
            reviewManager,
            settings,
            { 100L },
            { promptWasCalled = true },
        )

        controller.reviewPromptIsReady = true
        controller.promptReview(HomeActivity())
        assertEquals(100L, settings.lastReviewPromptTimeInMillis)
        assertTrue(promptWasCalled)
    }

    @Test
    fun promptReviewWillNotBeCalledIfNotReady() = runTest {
        var promptWasCalled = false
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L,
        )

        val controller = ReviewPromptController(
            reviewManager,
            settings,
            { 100L },
            { promptWasCalled = true },
        )

        controller.promptReview(HomeActivity())
        assertFalse(promptWasCalled)
    }

    @Test
    fun promptReviewWillUnreadyPromptAfterCalled() = runTest {
        var promptWasCalled = false
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L,
        )

        val controller = ReviewPromptController(
            reviewManager,
            settings,
            { 100L },
            { promptWasCalled = true },
        )

        controller.reviewPromptIsReady = true

        assertTrue(controller.reviewPromptIsReady)
        controller.promptReview(HomeActivity())

        assertFalse(controller.reviewPromptIsReady)
        assertTrue(promptWasCalled)
    }

    @Test
    fun trackApplicationLaunch() {
        val settings = TestReviewSettings(
            numberOfAppLaunches = 4,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L,
        )

        val controller = ReviewPromptController(
            reviewManager,
            settings,
            { 0L },
        )

        assertFalse(controller.reviewPromptIsReady)
        assertEquals(4, settings.numberOfAppLaunches)

        controller.trackApplicationLaunch()

        assertEquals(5, settings.numberOfAppLaunches)
        assertTrue(controller.reviewPromptIsReady)
    }

    @Test
    fun shouldShowPrompt() {
        val settings = TestReviewSettings(
            numberOfAppLaunches = 5,
            isDefault = true,
            lastReviewPromptTimeInMillis = 0L,
        )

        val controller = ReviewPromptController(
            reviewManager,
            settings,
            { TEST_TIME_NOW },
        )

        // Test first success criteria
        controller.reviewPromptIsReady = true
        assertTrue(controller.shouldShowPrompt())

        // Test with last prompt approx 4 months earlier
        settings.apply {
            numberOfAppLaunches = 5
            isDefault = true
            lastReviewPromptTimeInMillis = MORE_THAN_4_MONTHS_FROM_TEST_TIME_NOW
        }

        controller.reviewPromptIsReady = true
        assertTrue(controller.shouldShowPrompt())

        // Test without being the default browser
        settings.apply {
            numberOfAppLaunches = 5
            isDefault = false
            lastReviewPromptTimeInMillis = 0L
        }

        controller.reviewPromptIsReady = true
        assertFalse(controller.shouldShowPrompt())

        // Test with number of app launches < 5
        settings.apply {
            numberOfAppLaunches = 4
            isDefault = true
            lastReviewPromptTimeInMillis = 0L
        }

        controller.reviewPromptIsReady = true
        assertFalse(controller.shouldShowPrompt())

        // Test with last prompt less than 4 months ago
        settings.apply {
            numberOfAppLaunches = 5
            isDefault = true
            lastReviewPromptTimeInMillis = LESS_THAN_4_MONTHS_FROM_TEST_TIME_NOW
        }

        controller.reviewPromptIsReady = true
        assertFalse(controller.shouldShowPrompt())
    }

    @Test
    fun reviewPromptWasDisplayed() {
        testRecordReviewPromptEventRecordsTheExpectedData("isNoOp=false", "true")
    }

    @Test
    fun reviewPromptWasNotDisplayed() {
        testRecordReviewPromptEventRecordsTheExpectedData("isNoOp=true", "false")
    }

    @Test
    fun reviewPromptDisplayStateUnknown() {
        testRecordReviewPromptEventRecordsTheExpectedData(expected = "error")
    }

    private fun testRecordReviewPromptEventRecordsTheExpectedData(
        reviewInfoArg: String = "",
        expected: String,
    ) {
        val numberOfAppLaunches = 1
        val reviewInfoAsString =
            "ReviewInfo{pendingIntent=PendingIntent{5b613b1: android.os.BinderProxy@46c8096}, $reviewInfoArg}"
        val datetime = Date(TEST_TIME_NOW)
        val formattedNowLocalDatetime = SIMPLE_DATE_FORMAT.format(datetime)

        assertNull(ReviewPrompt.promptAttempt.testGetValue())
        recordReviewPromptEvent(reviewInfoAsString, numberOfAppLaunches, datetime)

        val reviewPromptData = ReviewPrompt.promptAttempt.testGetValue()!!.last().extra!!
        assertEquals(expected, reviewPromptData["prompt_was_displayed"])
        assertEquals(numberOfAppLaunches, reviewPromptData["number_of_app_launches"]!!.toInt())
        assertEquals(formattedNowLocalDatetime, reviewPromptData["local_datetime"])
    }

    companion object {
        private const val TEST_TIME_NOW = 1598416882805L
        private const val MORE_THAN_4_MONTHS_FROM_TEST_TIME_NOW = 1588048882804L
        private const val LESS_THAN_4_MONTHS_FROM_TEST_TIME_NOW = 1595824882905L
        private val SIMPLE_DATE_FORMAT by lazy {
            SimpleDateFormat(
                "yyyy-MM-dd'T'HH:mm:ss",
                Locale.getDefault(),
            )
        }
    }
}
