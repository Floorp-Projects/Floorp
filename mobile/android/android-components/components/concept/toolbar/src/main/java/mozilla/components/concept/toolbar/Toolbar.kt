/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

/**
 * Interface to be implemented by components that provide browser toolbar functionality.
 */
interface Toolbar {

    /**
     * Displays the given URL string as part of this Toolbar.
     *
     * @param url the url to display
     */
    fun displayUrl(url: String)

    /**
     * Registers the given function to be invoked when the url changes as result
     * of user input.
     *
     * @param listener the listener function
     */
    fun setOnUrlChangeListener(listener: (String) -> Unit)
}
