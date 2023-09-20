/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.shopping.ProductAnalysis
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * Service that handles the network requests for the review quality check feature.
 */
interface ReviewQualityCheckService {

    /**
     * Fetches the product review for the current tab.
     *
     * @return [ProductAnalysis] if the request succeeds, null otherwise.
     */
    suspend fun fetchProductReview(): ProductAnalysis?
}

/**
 * Service that handles the network requests for the review quality check feature.
 *
 * @property browserStore Reference to the application's [BrowserStore] to access state.
 */
class DefaultReviewQualityCheckService(
    private val browserStore: BrowserStore,
) : ReviewQualityCheckService {

    override suspend fun fetchProductReview(): ProductAnalysis? = withContext(Dispatchers.Main) {
        suspendCoroutine { continuation ->
            browserStore.state.selectedTab?.let { tab ->
                tab.engineState.engineSession?.requestProductAnalysis(
                    url = tab.content.url,
                    onResult = { continuation.resume(it) },
                    onException = { continuation.resume(null) },
                )
            }
        }
    }
}
