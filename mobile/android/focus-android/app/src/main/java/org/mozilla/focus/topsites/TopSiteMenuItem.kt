/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import mozilla.components.feature.top.sites.TopSite

/**
 * A menu item in the top site dropdown menu.
 *
 * @property title The menu item title.
 * @property onClick Invoked when the user clicks on the menu item.
 */
data class TopSiteMenuItem(
    val title: String,
    val onClick: (TopSite) -> Unit
)
