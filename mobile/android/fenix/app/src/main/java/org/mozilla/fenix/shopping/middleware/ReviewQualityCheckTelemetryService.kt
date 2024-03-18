/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.log.logger.Logger
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/**
 * Service that handles telemetry events for review checker.
 */
interface ReviewQualityCheckTelemetryService {

    /**
     * Sends a click attribution event for a given product aid.
     */
    suspend fun recordRecommendedProductClick(productAid: String): Unit?

    /**
     * Sends an impression attribution event for a given product aid.
     */
    suspend fun recordRecommendedProductImpression(productAid: String): Unit?
}

/**
 * Service that handles the network requests for the review quality check feature.
 *
 * @param browserStore Reference to the application's [BrowserStore] to access state.
 */
class DefaultReviewQualityCheckTelemetryService(
    private val browserStore: BrowserStore,
) : ReviewQualityCheckTelemetryService {

    private val logger = Logger(TAG)

    override suspend fun recordRecommendedProductClick(productAid: String) =
        withContext(Dispatchers.Main) {
            suspendCoroutine { continuation ->
                browserStore.state.selectedTab?.engineState?.engineSession?.sendClickAttributionEvent(
                    aid = productAid,
                    onResult = {
                        continuation.resume(Unit)
                    },
                    onException = {
                        logger.error("Error sending click attribution event", it)
                        continuation.resume(null)
                    },
                )
            }
        }

    override suspend fun recordRecommendedProductImpression(productAid: String) =
        withContext(Dispatchers.Main) {
            suspendCoroutine { continuation ->
                browserStore.state.selectedTab?.engineState?.engineSession?.sendImpressionAttributionEvent(
                    aid = productAid,
                    onResult = {
                        continuation.resume(Unit)
                    },
                    onException = {
                        logger.error("Error sending impression attribution event", it)
                        continuation.resume(null)
                    },
                )
            }
        }

    companion object {
        private const val TAG = "ReviewQualityCheckTelemetryService"
    }
}
