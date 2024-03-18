/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState

/**
 * Contract for how all user interactions with the Inactive Tabs feature are to be handled.
 */
interface InactiveTabsController {

    /**
     * Opens the provided inactive tab.
     *
     * @param tab [TabSessionState] that was clicked.
     */
    fun handleInactiveTabClicked(tab: TabSessionState)

    /**
     * Closes the provided inactive tab.
     *
     * @param tab [TabSessionState] that was clicked.
     */
    fun handleCloseInactiveTabClicked(tab: TabSessionState)

    /**
     * Expands or collapses the inactive tabs section.
     *
     * @param expanded true when the tap should expand the inactive section.
     */
    fun handleInactiveTabsHeaderClicked(expanded: Boolean)

    /**
     * Dismisses the inactive tabs auto-close dialog.
     */
    fun handleInactiveTabsAutoCloseDialogDismiss()

    /**
     * Enables the inactive tabs auto-close feature with a default time period.
     */
    fun handleEnableInactiveTabsAutoCloseClicked()

    /**
     * Deletes all inactive tabs.
     */
    fun handleDeleteAllInactiveTabsClicked()
}
