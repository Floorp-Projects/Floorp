/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.ktx.kotlin.getRepresentativeCharacter
import mozilla.components.ui.colors.PhotonColors

/**
 * A list of top sites.
 *
 * @param topSites List of [TopSite] to display.
 * @param menuItems List of [TopSiteMenuItem] to display in a top site dropdown menu.
 * @param onTopSiteClick Invoked when the user clicks on a top site.
 */
@Composable
fun TopSites(
    topSites: List<TopSite>,
    menuItems: List<TopSiteMenuItem>,
    onTopSiteClick: (TopSite) -> Unit = {}
) {
    Row(
        modifier = Modifier
            .padding(horizontal = 10.dp)
            .size(width = 324.dp, height = 84.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(28.dp)
    ) {
        topSites.forEach { topSite ->
            TopSiteItem(
                topSite = topSite,
                menuItems = menuItems,
                onTopSiteClick = onTopSiteClick
            )
        }
    }
}

/**
 * A top site item.
 *
 * @param topSite The [TopSite] to display.
 * @param menuItems List of [TopSiteMenuItem] to display in a top site dropdown menu.
 * @param onTopSiteClick Invoked when the user clicks on a top site.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun TopSiteItem(
    topSite: TopSite,
    menuItems: List<TopSiteMenuItem>,
    onTopSiteClick: (TopSite) -> Unit = {}
) {
    var menuExpanded by remember { mutableStateOf(false) }

    Box {
        Column(
            modifier = Modifier
                .combinedClickable(
                    onClick = { onTopSiteClick(topSite) },
                    onLongClick = { menuExpanded = true }
                )
                .size(width = 60.dp, height = 84.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            TopSiteFaviconCard(topSite = topSite)

            Text(
                text = topSite.title ?: topSite.url,
                modifier = Modifier.padding(top = 8.dp),
                color = PhotonColors.LightGrey05,
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1
            )
        }

        DropdownMenu(
            expanded = menuExpanded,
            onDismissRequest = { menuExpanded = false },
            modifier = Modifier.background(color = PhotonColors.Ink30)
        ) {
            for (item in menuItems) {
                DropdownMenuItem(
                    onClick = {
                        menuExpanded = false
                        item.onClick(topSite)
                    }
                ) {
                    Text(
                        text = item.title,
                        color = Color.White
                    )
                }
            }
        }
    }
}

/**
 * The top site favicon card.
 *
 * @param topSite The [TopSite] to display.
 */
@Composable
private fun TopSiteFaviconCard(topSite: TopSite) {
    Card(
        modifier = Modifier.size(60.dp),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = PhotonColors.Ink30
    ) {
        Column(
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Surface(
                modifier = Modifier.size(36.dp),
                shape = RoundedCornerShape(4.dp),
                color = PhotonColors.Ink60
            ) {
                Column(
                    verticalArrangement = Arrangement.Center,
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        text = topSite.url.getRepresentativeCharacter(),
                        color = PhotonColors.LightGrey05,
                        fontSize = 20.sp
                    )
                }
            }
        }
    }
}
