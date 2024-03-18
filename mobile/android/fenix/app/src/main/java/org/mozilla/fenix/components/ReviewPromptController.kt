/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Activity
import androidx.annotation.VisibleForTesting
import com.google.android.play.core.review.ReviewInfo
import com.google.android.play.core.review.ReviewManager
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.withContext
import org.mozilla.fenix.GleanMetrics.ReviewPrompt
import org.mozilla.fenix.utils.Settings
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Interface that describes the settings needed to track the Review Prompt.
 */
interface ReviewSettings {
    var numberOfAppLaunches: Int
    val isDefaultBrowser: Boolean
    var lastReviewPromptTimeInMillis: Long
}

/**
 * Wraps `Settings` to conform to `ReviewSettings`.
 */
class FenixReviewSettings(
    val settings: Settings,
) : ReviewSettings {
    override var numberOfAppLaunches: Int
        get() = settings.numberOfAppLaunches
        set(value) { settings.numberOfAppLaunches = value }
    override val isDefaultBrowser: Boolean
        get() = settings.isDefaultBrowserBlocking()
    override var lastReviewPromptTimeInMillis: Long
        get() = settings.lastReviewPromptTimeInMillis
        set(value) { settings.lastReviewPromptTimeInMillis = value }
}

/**
 * Controls the Review Prompt behavior.
 */
class ReviewPromptController(
    private val manager: ReviewManager,
    private val reviewSettings: ReviewSettings,
    private val timeNowInMillis: () -> Long = { System.currentTimeMillis() },
    private val tryPromptReview: suspend (Activity) -> Unit = { activity ->
        val flow = manager.requestReviewFlow()

        withContext(Main) {
            flow.addOnCompleteListener {
                if (it.isSuccessful) {
                    manager.launchReviewFlow(activity, it.result)
                    recordReviewPromptEvent(
                        it.result.toString(),
                        reviewSettings.numberOfAppLaunches,
                        Date(),
                    )
                }
            }
        }
    },
) {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Volatile
    var reviewPromptIsReady = false

    suspend fun promptReview(activity: Activity) {
        if (shouldShowPrompt()) {
            tryPromptReview(activity)
            reviewSettings.lastReviewPromptTimeInMillis = timeNowInMillis()
        }
    }

    fun trackApplicationLaunch() {
        reviewSettings.numberOfAppLaunches = reviewSettings.numberOfAppLaunches + 1
        // We only want to show the the prompt after we've finished "launching" the application.
        reviewPromptIsReady = true
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun shouldShowPrompt(): Boolean {
        if (!reviewPromptIsReady) {
            return false
        } else {
            // We only want to try to show it once to avoid unnecessary disk reads
            reviewPromptIsReady = false
        }

        if (!reviewSettings.isDefaultBrowser) { return false }

        val hasOpenedFiveTimes = reviewSettings.numberOfAppLaunches >= NUMBER_OF_LAUNCHES_REQUIRED
        val now = timeNowInMillis()
        val apprxFourMonthsAgo = now - (APPRX_MONTH_IN_MILLIS * NUMBER_OF_MONTHS_TO_PASS)
        val lastPrompt = reviewSettings.lastReviewPromptTimeInMillis
        val hasNotBeenPromptedLastFourMonths = lastPrompt == 0L || lastPrompt <= apprxFourMonthsAgo

        return hasOpenedFiveTimes && hasNotBeenPromptedLastFourMonths
    }

    companion object {
        private const val APPRX_MONTH_IN_MILLIS: Long = 1000L * 60L * 60L * 24L * 30L
        private const val NUMBER_OF_LAUNCHES_REQUIRED = 5
        private const val NUMBER_OF_MONTHS_TO_PASS = 4
    }
}

/**
 * Records a [ReviewPrompt] with the required data.
 *
 * **Note:** The docs for [ReviewManager.launchReviewFlow] state 'In some circumstances the review
 * flow will not be shown to the user, e.g. they have already seen it recently, so do not assume that
 * calling this method will always display the review dialog.'
 * However, investigation has shown that a [ReviewInfo] instance with the flag:
 * - 'isNoOp=true' indicates that the prompt has NOT been displayed.
 * - 'isNoOp=false' indicates that a prompt has been displayed.
 * [ReviewManager.launchReviewFlow] will modify the ReviewInfo instance which can be used to determine
 * which of these flags is present.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
fun recordReviewPromptEvent(
    reviewInfoAsString: String,
    numberOfAppLaunches: Int,
    now: Date,
) {
    val formattedLocalDatetime =
        SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.getDefault()).format(now)

    // The internals of ReviewInfo cannot be accessed directly or cast nicely, so lets simply use
    // the object as a string.
    // ReviewInfo is susceptible to changes outside of our control hence the catch-all 'else' statement.
    val promptWasDisplayed = if (reviewInfoAsString.contains("isNoOp=true")) {
        "false"
    } else if (reviewInfoAsString.contains("isNoOp=false")) {
        "true"
    } else {
        "error"
    }

    ReviewPrompt.promptAttempt.record(
        ReviewPrompt.PromptAttemptExtra(
            promptWasDisplayed = promptWasDisplayed,
            localDatetime = formattedLocalDatetime,
            numberOfAppLaunches = numberOfAppLaunches,
        ),
    )
}
