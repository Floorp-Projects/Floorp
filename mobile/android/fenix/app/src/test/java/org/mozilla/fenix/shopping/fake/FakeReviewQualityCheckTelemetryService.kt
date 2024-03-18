/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.fake

import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckTelemetryService

class FakeReviewQualityCheckTelemetryService(
    private val recordClick: (String) -> Unit = {},
    private val recordImpression: (String) -> Unit = {},
) : ReviewQualityCheckTelemetryService {

    override suspend fun recordRecommendedProductClick(productAid: String) {
        return recordClick.invoke(productAid)
    }

    override suspend fun recordRecommendedProductImpression(productAid: String) {
        return recordImpression.invoke(productAid)
    }
}
