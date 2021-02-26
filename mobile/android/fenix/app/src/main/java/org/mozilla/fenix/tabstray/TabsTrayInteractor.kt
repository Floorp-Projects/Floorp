/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

interface TabsTrayInteractor {
    /**
     * Set the current tray item to the clamped [position].
     */
    fun setCurrentTrayPosition(position: Int)

    /**
     * Dismisses the tabs tray and navigates to the browser.
     */
    fun navigateToBrowser()

    /**
     * Invoked when a tab is removed from the tabs tray with the given [sessionId].
     */
    fun tabRemoved(sessionId: String)
}
