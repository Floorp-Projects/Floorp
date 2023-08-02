/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.mozilla.fenix.utils.Settings

/**
 * Interface to get and set preferences for the review quality check feature.
 */
interface ReviewQualityCheckPreferences {
    /**
     * Returns true if the user has opted in to the review quality check feature.
     */
    suspend fun enabled(): Boolean

    /**
     * Returns true if the user has enabled product recommendations.
     */
    suspend fun productRecommendationsEnabled(): Boolean

    /**
     * Sets whether the user has opted in to the review quality check feature.
     */
    suspend fun setEnabled(isEnabled: Boolean)

    /**
     * Sets whether the user has enabled product recommendations.
     */
    suspend fun setProductRecommendationsEnabled(isEnabled: Boolean)
}

/**
 * Implementation of [ReviewQualityCheckPreferences] that uses [Settings] to store/fetch
 * preferences.
 *
 * @param settings The [Settings] instance to use.
 */
class ReviewQualityCheckPreferencesImpl(
    private val settings: Settings,
) : ReviewQualityCheckPreferences {

    override suspend fun enabled(): Boolean = withContext(Dispatchers.IO) {
        settings.isReviewQualityCheckEnabled
    }

    override suspend fun productRecommendationsEnabled(): Boolean = withContext(Dispatchers.IO) {
        settings.isReviewQualityCheckProductRecommendationsEnabled
    }

    override suspend fun setEnabled(isEnabled: Boolean) {
        settings.isReviewQualityCheckEnabled = isEnabled
    }

    override suspend fun setProductRecommendationsEnabled(isEnabled: Boolean) {
        settings.isReviewQualityCheckProductRecommendationsEnabled = isEnabled
    }
}
