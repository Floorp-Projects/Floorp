/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.history

import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType

/**
 * An Interface for providing a paginated list of [VisitInfo]
 */
interface PagedHistoryProvider {
    /**
     * Gets a list of [VisitInfo]
     * @param offset How much to offset the list by
     * @param numberOfItems How many items to fetch
     * @param onComplete A callback that returns the list of [VisitInfo]
     */
    fun getHistory(offset: Long, numberOfItems: Long, onComplete: (List<VisitInfo>) -> Unit)
}

// A PagedList DataSource runs on a background thread automatically.
// If we run this in our own coroutineScope it breaks the PagedList
fun HistoryStorage.createSynchronousPagedHistoryProvider(): PagedHistoryProvider {
    return object : PagedHistoryProvider {

        override fun getHistory(
            offset: Long,
            numberOfItems: Long,
            onComplete: (List<VisitInfo>) -> Unit
        ) {
            runBlocking {
                val history = getVisitsPaginated(
                    offset,
                    numberOfItems,
                    excludeTypes = listOf(
                        VisitType.NOT_A_VISIT,
                        VisitType.DOWNLOAD,
                        VisitType.REDIRECT_TEMPORARY,
                        VisitType.RELOAD,
                        VisitType.EMBED,
                        VisitType.FRAMED_LINK,
                        VisitType.REDIRECT_PERMANENT
                    )
                )

                onComplete(history)
            }
        }
    }
}
