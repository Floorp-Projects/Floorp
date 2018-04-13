/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

/**
 * Interface to be implemented by components that provide browser toolbar functionality.
 */
interface Toolbar {
    /**
     * Display the given URL.
     */
    fun displayUrl(url: String)
}
