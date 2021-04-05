/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentDirections

interface TabsTrayController {

    /**
     * Called when user clicks the new tab button.
     */
    fun onNewTabTapped(isPrivate: Boolean)
}

class DefaultTabsTrayController(
    private val browsingModeManager: BrowsingModeManager,
    private val navController: NavController
) : TabsTrayController {

    override fun onNewTabTapped(isPrivate: Boolean) {
        browsingModeManager.mode = BrowsingMode.fromBoolean(isPrivate)
        navController.navigate(TabTrayDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true))
    }
}
