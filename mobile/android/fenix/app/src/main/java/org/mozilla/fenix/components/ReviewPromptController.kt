/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Activity
import androidx.annotation.VisibleForTesting
import com.google.android.play.core.review.ReviewManager
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.withContext
import org.mozilla.fenix.utils.Settings

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
    val settings: Settings
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
                }
            }
        }
    }
) {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Volatile var reviewPromptIsReady = false

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
