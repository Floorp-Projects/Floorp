/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.history

import kotlinx.coroutines.Deferred

/**
 * An interface used for providing history information to an engine (e.g. for link highlighting),
 * and receiving history updates from the engine (visits to URLs, title changes).
 *
 * Even though this interface is defined at the "concept" layer, its get* methods are tailored to
 * two types of engines which we support (system's WebView and GeckoView).
 */
interface HistoryTrackingDelegate {
    /**
     * A URI visit happened that an engine considers worthy of being recorded in browser's history.
     */
    suspend fun onVisited(uri: String, isReload: Boolean = false, privateMode: Boolean)

    /**
     * Title changed for a given URI.
     */
    suspend fun onTitleChanged(uri: String, title: String, privateMode: Boolean)

    /**
     * An engine needs to know "visited" (true/false) status for provided URIs.
     */
    fun getVisited(uris: List<String>, privateMode: Boolean): Deferred<List<Boolean>>

    /**
     * An engine needs to know a list of all visited URIs.
     */
    fun getVisited(privateMode: Boolean): Deferred<List<String>>
}
