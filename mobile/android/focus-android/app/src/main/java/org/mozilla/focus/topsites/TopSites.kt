/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.ktx.kotlin.getRepresentativeCharacter
import org.mozilla.focus.R
import org.mozilla.focus.ui.menu.CustomDropdownMenu
import org.mozilla.focus.ui.menu.MenuItem
import org.mozilla.focus.ui.theme.focusColors

/**
 * A list of top sites.
 *
 * @param topSites List of [TopSite] to display.
 * @param onTopSiteClicked Invoked when the user clicked the top site
 * @param onRemoveTopSiteClicked Invoked when the user clicked 'Remove' item from drop down menu
 * @param onRenameTopSiteClicked Invoked when the user clicked 'Rename' item from drop down menu
 */

@Composable
fun TopSites(
    topSites: List<TopSite>,
    onTopSiteClicked: (TopSite) -> Unit,
    onRemoveTopSiteClicked: (TopSite) -> Unit,
    onRenameTopSiteClicked: (TopSite) -> Unit,
) {
    Row(
        modifier = Modifier
            .padding(horizontal = 10.dp)
            .size(width = 324.dp, height = 84.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(28.dp),
    ) {
        topSites.forEach { topSite ->
            TopSiteItem(
                topSite = topSite,
                menuItems = listOfNotNull(
                    MenuItem(
                        title = stringResource(R.string.rename_top_site_item),
                        onClick = { onRenameTopSiteClicked(topSite) },
                    ),
                    MenuItem(
                        title = stringResource(R.string.remove_top_site),
                        onClick = { onRemoveTopSiteClicked(topSite) },
                    ),
                ),
                onTopSiteClick = { item -> onTopSiteClicked(item) },
            )
        }
    }
}

/**
 * A top site item.
 *
 * @param topSite The [TopSite] to display.
 * @param menuItems List of [MenuItem] to display in a top site dropdown menu.
 * @param onTopSiteClick Invoked when the user clicks on a top site.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun TopSiteItem(
    topSite: TopSite,
    menuItems: List<MenuItem>,
    onTopSiteClick: (TopSite) -> Unit = {},
) {
    var menuExpanded by remember { mutableStateOf(false) }

    Box {
        Column(
            modifier = Modifier
                .combinedClickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = { onTopSiteClick(topSite) },
                    onLongClick = { menuExpanded = true },
                )
                .size(width = 60.dp, height = 84.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            TopSiteFaviconCard(topSite = topSite)

            Text(
                text = topSite.title ?: topSite.url,
                modifier = Modifier.padding(top = 8.dp),
                color = focusColors.topSiteTitle,
                fontSize = 12.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )

            CustomDropdownMenu(
                menuItems = menuItems,
                isExpanded = menuExpanded,
                onDismissClicked = { menuExpanded = false },
            )
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
        backgroundColor = focusColors.topSiteBackground,
    ) {
        Column(
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Surface(
                modifier = Modifier.size(36.dp),
                shape = RoundedCornerShape(4.dp),
                color = focusColors.surface,
            ) {
                Column(
                    verticalArrangement = Arrangement.Center,
                    horizontalAlignment = Alignment.CenterHorizontally,
                ) {
                    Text(
                        text = if (topSite.title.isNullOrEmpty()) {
                            topSite.url.getRepresentativeCharacter()
                        } else {
                            topSite.title?.get(0).toString()
                        },
                        color = focusColors.topSiteFaviconText,
                        fontSize = 20.sp,
                    )
                }
            }
        }
    }
}
