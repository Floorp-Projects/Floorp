/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.fake

import mozilla.components.browser.engine.gecko.shopping.GeckoProductAnalysis
import org.mozilla.fenix.shopping.middleware.AnalysisStatusDto
import org.mozilla.fenix.shopping.middleware.ReviewQualityCheckService

class FakeReviewQualityCheckService(
    private val productAnalysis: (Int) -> GeckoProductAnalysis? = { null },
    private val reanalysis: AnalysisStatusDto? = null,
    private val status: AnalysisStatusDto? = null,
    private val selectedTabUrl: String? = null,
) : ReviewQualityCheckService {

    private var analysisCount = 0

    override suspend fun fetchProductReview(): GeckoProductAnalysis? {
        return productAnalysis(analysisCount).also {
            analysisCount++
        }
    }

    override suspend fun reanalyzeProduct(): AnalysisStatusDto? = reanalysis

    override suspend fun analysisStatus(): AnalysisStatusDto? = status

    override fun selectedTabUrl(): String? = selectedTabUrl
}
