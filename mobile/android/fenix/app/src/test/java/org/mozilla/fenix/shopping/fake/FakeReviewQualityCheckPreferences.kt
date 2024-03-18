/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.fake

import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckPreferences

class FakeReviewQualityCheckPreferences(
    private val isEnabled: Boolean = false,
    private val isProductRecommendationsEnabled: Boolean? = false,
    private val updateCFRCallback: () -> Unit = { },
) : ReviewQualityCheckPreferences {
    override suspend fun enabled(): Boolean = isEnabled

    override suspend fun productRecommendationsEnabled(): Boolean? = isProductRecommendationsEnabled

    override suspend fun setEnabled(isEnabled: Boolean) {
    }

    override suspend fun setProductRecommendationsEnabled(isEnabled: Boolean) {
    }

    override suspend fun updateCFRCondition(time: Long) {
        updateCFRCallback()
    }
}
