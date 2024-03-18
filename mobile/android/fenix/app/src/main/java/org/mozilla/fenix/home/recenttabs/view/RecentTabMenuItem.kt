/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import org.mozilla.fenix.home.recenttabs.RecentTab

/**
* A menu item in the recent tab dropdown menu.
*
* @property title The menu item title.
* @property onClick Invoked when the user clicks on the menu item.
*/
class RecentTabMenuItem(
    val title: String,
    val onClick: (RecentTab.Tab) -> Unit,
)
