/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

/**
 * Interface to be implemented by components that provide hiding-on-scroll toolbar functionality.
 */
interface ScrollableToolbar {

    /**
     * Enable scrolling of the dynamic toolbar. Restore this functionality after [disableScrolling] stopped it.
     *
     * The toolbar may have other intrinsic checks depending on which the toolbar will be animated or not.
     */
    fun enableScrolling()

    /**
     * Completely disable scrolling of the dynamic toolbar.
     * Use [enableScrolling] to restore the functionality.
     */
    fun disableScrolling()

    /**
     * Force the toolbar to expand.
     */
    fun expand()

    /**
     * Force the toolbar to collapse. Only if dynamic.
     */
    fun collapse()
}
