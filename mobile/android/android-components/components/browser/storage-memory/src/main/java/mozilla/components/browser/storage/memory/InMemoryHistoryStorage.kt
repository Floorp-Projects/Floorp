/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.memory

import android.support.annotation.VisibleForTesting
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.PageObservation
import mozilla.components.concept.storage.VisitType

data class Visit(val timestamp: Long, val type: VisitType)

/**
 * An in-memory implementation of [mozilla.components.concept.storage.HistoryStorage].
 */
class InMemoryHistoryStorage : HistoryStorage {
    @VisibleForTesting
    internal val pages: LinkedHashMap<String, MutableList<Visit>> = linkedMapOf()
    @VisibleForTesting
    internal val pageMeta: HashMap<String, PageObservation> = hashMapOf()

    override fun recordVisit(uri: String, visitType: VisitType) {
        val now = System.currentTimeMillis()

        synchronized(pages) {
            if (!pages.containsKey(uri)) {
                pages[uri] = mutableListOf(Visit(now, visitType))
            } else {
                pages[uri]!!.add(Visit(now, visitType))
            }
        }
    }

    override fun recordObservation(uri: String, observation: PageObservation) {
        synchronized(pageMeta) {
            pageMeta[uri] = observation
        }
    }

    override fun getVisited(uris: List<String>, callback: (List<Boolean>) -> Unit) {
        callback(synchronized(pages) {
            uris.map {
                if (pages[it] != null && pages[it]!!.size > 0) {
                    return@map true
                }
                return@map false
            }
        })
    }

    override fun getVisited(callback: (List<String>) -> Unit) {
        callback(synchronized(pages) {
            (pages.keys.toList())
        })
    }
}
