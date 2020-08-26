/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Activity
import android.content.Context
import androidx.annotation.VisibleForTesting
import com.google.android.play.core.ktx.launchReview
import com.google.android.play.core.ktx.requestReview
import com.google.android.play.core.review.ReviewManagerFactory
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
): ReviewSettings {
    override var numberOfAppLaunches: Int
        get() = settings.numberOfAppLaunches
        set(value) { settings.numberOfAppLaunches = value }
    override val isDefaultBrowser: Boolean
        get() = settings.isDefaultBrowser()
    override var lastReviewPromptTimeInMillis: Long
        get() = settings.lastReviewPromptTimeInMillis
        set(value) { settings.lastReviewPromptTimeInMillis = value }
}

/**
 * Controls the Review Prompt behavior.
 */
class ReviewPromptController(
    private val context: Context,
    private val reviewSettings: ReviewSettings,
    private val timeNowInMillis: () -> Long = { System.currentTimeMillis() }
) {
    suspend fun promptReview(activity: Activity) {
        if (shouldShowPrompt()) {
            val manager = ReviewManagerFactory.create(context)
            val reviewInfo = manager.requestReview()
            manager.launchReview(activity, reviewInfo)

            reviewSettings.lastReviewPromptTimeInMillis = timeNowInMillis()
        }
    }

    fun trackApplicationLaunch() {
        reviewSettings.numberOfAppLaunches = reviewSettings.numberOfAppLaunches + 1
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun shouldShowPrompt(): Boolean {
        if (!reviewSettings.isDefaultBrowser) { return false }

        val hasOpenedFiveTimes = reviewSettings.numberOfAppLaunches >= 5
        val apprxFourMonthsAgo = timeNowInMillis() - (APPRX_MONTH_IN_MILLIS * 4)
        val hasNotBeenPromptedLastFourMonths = reviewSettings.lastReviewPromptTimeInMillis <= apprxFourMonthsAgo

        return hasOpenedFiveTimes && hasNotBeenPromptedLastFourMonths
    }

    companion object {
        private const val APPRX_MONTH_IN_MILLIS: Long = 1000L * 60L * 60L * 24L * 30L
    }
}