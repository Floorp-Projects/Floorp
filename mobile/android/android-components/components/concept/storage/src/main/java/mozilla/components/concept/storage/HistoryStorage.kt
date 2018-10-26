/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

/**
 * An interface which defines read/write methods for history data.
 */
interface HistoryStorage {
    /**
     * Records a visit to a page.
     * @param uri of the page which was visited.
     * @param visitType type of the visit, see [VisitType].
     */
    fun recordVisit(uri: String, visitType: VisitType)

    /**
     * Records an observation about a page.
     * @param uri of the page for which to record an observation.
     * @param observation a [PageObservation] which encapsulates meta data observed about the page.
     */
    fun recordObservation(uri: String, observation: PageObservation)

    /**
     * Maps a list of page URIs to a list of booleans indicating if each URI was visited.
     * @param uris a list of page URIs about which "visited" information is being requested.
     * @param callback will be invoked with a list of booleans indicating visited status of each
     * corresponding page URI from [uris].
     */
    fun getVisited(uris: List<String>, callback: (List<Boolean>) -> Unit)

    /**
     * Retrieves a list of all visited pages.
     * @param callback will be invoked with a list of all visited page URIs.
     */
    fun getVisited(callback: (List<String>) -> Unit)
}

data class PageObservation(val title: String?)

/**
 * Visit type constants as defined by Desktop Firefox.
 */
@SuppressWarnings("MagicNumber")
enum class VisitType(val type: Int) {
    // User followed a link.
    LINK(1),
    // User typed a URL or selected it from the UI (autocomplete results, etc).
    TYPED(2),
    RELOAD(9)
}
