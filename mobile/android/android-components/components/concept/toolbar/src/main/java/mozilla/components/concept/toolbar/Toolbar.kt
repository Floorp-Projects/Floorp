/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.support.annotation.DrawableRes

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
     * Displays the currently used search terms as part of this Toolbar.
     *
     * @param searchTerms the search terms used by the current session
     */
    fun setSearchTerms(searchTerms: String)

    /**
     * Displays the given loading progress. Expects values in the range [0, 100].
     */
    fun displayProgress(progress: Int)

    /**
     * Should be called by an activity when the user pressed the back key of the device.
     *
     * @return Returns true if the back press event was handled and should not be propagated further.
     */
    fun onBackPressed(): Boolean

    /**
     * Registers the given function to be invoked when the url changes as result
     * of user input.
     *
     * @param listener the listener function
     */
    fun setOnUrlChangeListener(listener: (String) -> Unit)

    /**
     * Adds an action to be displayed on the right side of the toolbar in display mode.
     */
    fun addDisplayAction(action: Action)

    /**
     * An action button to be added to the toolbar.
     */
    data class Action(
        @DrawableRes val imageResource: Int,
        val contentDescription: String,
        val listener: () -> Unit
    )
}
