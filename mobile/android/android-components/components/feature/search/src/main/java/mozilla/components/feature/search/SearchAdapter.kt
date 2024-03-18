/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search

/**
 * May be implemented by client code in order to allow a component to start searches.
 */
interface SearchAdapter {

    /**
     * Called by the component to indicate that the user should be shown a search.
     */
    fun sendSearch(isPrivate: Boolean, text: String)

    /**
     * Called by the component to check whether or not the currently selected session is private.
     */
    fun isPrivateSession(): Boolean
}
