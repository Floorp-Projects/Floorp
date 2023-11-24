/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.fake

import mozilla.components.concept.engine.shopping.ProductAnalysis
import mozilla.components.concept.engine.shopping.ProductRecommendation
import org.mozilla.fenix.shopping.middleware.AnalysisStatusDto
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckService

class FakeReviewQualityCheckService(
    private val productAnalysis: (Int) -> ProductAnalysis? = { null },
    private val reanalysis: AnalysisStatusDto? = null,
    private val status: AnalysisStatusDto? = null,
    private val productRecommendation: () -> ProductRecommendation? = { null },
    private val recordClick: (String) -> Unit = {},
    private val recordImpression: (String) -> Unit = {},
) : ReviewQualityCheckService {

    private var analysisCount = 0

    override suspend fun fetchProductReview(): ProductAnalysis? {
        return productAnalysis(analysisCount).also {
            analysisCount++
        }
    }

    override suspend fun reanalyzeProduct(): AnalysisStatusDto? = reanalysis

    override suspend fun analysisStatus(): AnalysisStatusDto? = status

    override suspend fun productRecommendation(shouldRecordAvailableTelemetry: Boolean): ProductRecommendation? {
        return productRecommendation.invoke()
    }

    override suspend fun recordRecommendedProductClick(productAid: String) {
        recordClick(productAid)
    }

    override suspend fun recordRecommendedProductImpression(productAid: String) {
        recordImpression(productAid)
    }
}
