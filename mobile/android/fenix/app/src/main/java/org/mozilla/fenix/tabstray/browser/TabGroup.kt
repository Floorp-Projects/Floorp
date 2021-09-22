/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState

data class TabGroup(
    /**
     * The search term used for the tab group.
     */
    val searchTerm: String,

    /**
     * The list of tabSessionStates belonging to this tab group.
     */
    val tabs: List<TabSessionState>,

    /**
     * The last time tabs in this group was accessed.
     */
    val lastAccess: Long
)
