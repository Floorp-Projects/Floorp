/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.home

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.focus.topsites.TopSiteMenuItem
import org.mozilla.focus.topsites.TopSites

/**
 * The home screen.
 *
 * @param topSites List of [TopSite] to display.
 * @param topSitesMenuItems List of [TopSiteMenuItem] to display in a top site dropdown menu.
 * @param onTopSiteClick Invoked when the user clicks on a top site.
 */
@Composable
fun HomeScreen(
    topSites: List<TopSite>,
    topSitesMenuItems: List<TopSiteMenuItem>,
    onTopSiteClick: (TopSite) -> Unit = {}
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        if (topSites.isNotEmpty()) {
            Spacer(modifier = Modifier.height(24.dp))

            TopSites(
                topSites = topSites,
                menuItems = topSitesMenuItems,
                onTopSiteClick = onTopSiteClick
            )
        }
    }
}
