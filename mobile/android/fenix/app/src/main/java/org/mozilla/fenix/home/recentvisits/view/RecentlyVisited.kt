/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.view

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.scrollable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Divider
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.theme.FirefoxTheme

// Number of recently visited items per column.
private const val VISITS_PER_COLUMN = 3

/**
 * A list of recently visited items.
 *
 * @param recentVisits List of [RecentHistoryGroup] to display.
 * @param menuItems List of [RecentVisitMenuItem] for [RecentHistoryGroup]s.
 * Currently [RecentHistoryHighlight]s do not support a menu -
 * https://mozilla-hub.atlassian.net/browse/FXMUX-187
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@Composable
fun RecentlyVisited(
    recentVisits: List<RecentlyVisitedItem>,
    menuItems: List<RecentVisitMenuItem>,
    onRecentVisitClick: (RecentlyVisitedItem, Int) -> Unit = { _, _ -> }
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.layer2,
        elevation = 6.dp
    ) {
        LazyRow(
            contentPadding = PaddingValues(16.dp),
            horizontalArrangement = Arrangement.spacedBy(32.dp)
        ) {
            val itemsList = recentVisits.chunked(VISITS_PER_COLUMN)

            itemsIndexed(itemsList) { pageIndex, items ->
                Column(
                    modifier = Modifier.fillMaxWidth()
                ) {
                    items.forEachIndexed { index, recentVisit ->
                        when (recentVisit) {
                            is RecentHistoryHighlight -> RecentlyVisitedHistoryHighlight(
                                recentVisit = recentVisit,
                                menuItems = menuItems,
                                showDividerLine = index < items.size - 1,
                                onRecentVisitClick = {
                                    onRecentVisitClick(it, pageIndex + 1)
                                }
                            )
                            is RecentHistoryGroup -> RecentlyVisitedHistoryGroup(
                                recentVisit = recentVisit,
                                menuItems = menuItems,
                                showDividerLine = index < items.size - 1,
                                onRecentVisitClick = {
                                    onRecentVisitClick(it, pageIndex + 1)
                                }
                            )
                        }
                    }
                }
            }
        }
    }
}

/**
 * A recently visited history group.
 *
 * @param recentVisit The [RecentHistoryGroup] to display.
 * @param menuItems List of [RecentVisitMenuItem] to display in a recent visit dropdown menu.
 * @param showDividerLine Whether to show a divider line at the bottom.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentlyVisitedHistoryGroup(
    recentVisit: RecentHistoryGroup,
    menuItems: List<RecentVisitMenuItem>,
    showDividerLine: Boolean,
    onRecentVisitClick: (RecentHistoryGroup) -> Unit = { _ -> },
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier
            .combinedClickable(
                onClick = { onRecentVisitClick(recentVisit) },
                onLongClick = { isMenuExpanded = true }
            )
            .size(268.dp, 56.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Image(
            painter = painterResource(R.drawable.ic_multiple_tabs),
            contentDescription = null,
            modifier = Modifier.size(24.dp),
        )

        Spacer(modifier = Modifier.width(16.dp))

        Column(
            modifier = Modifier.fillMaxSize()
        ) {
            RecentlyVisitedTitle(
                text = recentVisit.title,
                modifier = Modifier.padding(top = 7.dp, bottom = 2.dp)
            )

            RecentlyVisitedCaption(recentVisit.historyMetadata.size)

            if (showDividerLine) {
                RecentlyVisitedDivider(modifier = Modifier.padding(top = 9.dp))
            }
        }

        RecentlyVisitedMenu(
            showMenu = isMenuExpanded,
            menuItems = menuItems,
            recentVisit = recentVisit,
            onDismissRequest = { isMenuExpanded = false }
        )
    }
}

/**
 * A recently visited history item.
 *
 * @param recentVisit The [RecentHistoryHighlight] to display.
 * @param menuItems List of [RecentVisitMenuItem] to display in a recent visit dropdown menu.
 * @param showDividerLine Whether to show a divider line at the bottom.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentlyVisitedHistoryHighlight(
    recentVisit: RecentHistoryHighlight,
    menuItems: List<RecentVisitMenuItem>,
    showDividerLine: Boolean,
    onRecentVisitClick: (RecentHistoryHighlight) -> Unit = { _ -> },
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier
            .combinedClickable(
                onClick = { onRecentVisitClick(recentVisit) },
                onLongClick = { isMenuExpanded = true }
            )
            .size(268.dp, 56.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Favicon(url = recentVisit.url, size = 24.dp)

        Spacer(modifier = Modifier.width(16.dp))

        Box(modifier = Modifier.fillMaxSize()) {
            RecentlyVisitedTitle(
                text = recentVisit.title,
                modifier = Modifier.align(Alignment.CenterStart)
            )

            if (showDividerLine) {
                RecentlyVisitedDivider(modifier = Modifier.align(Alignment.BottomCenter))
            }
        }

        RecentlyVisitedMenu(
            showMenu = isMenuExpanded,
            menuItems = menuItems,
            recentVisit = recentVisit,
            onDismissRequest = { isMenuExpanded = false }
        )
    }
}

/**
 * The title of a recent visit.
 *
 * @param text [String] that will be display. Will be ellipsized if cannot fit on one line.
 * @param modifier [Modifier] allowing to perfectly place this.
 */
@Composable
private fun RecentlyVisitedTitle(
    text: String,
    modifier: Modifier = Modifier
) {
    Text(
        text = text,
        modifier = modifier,
        color = FirefoxTheme.colors.textPrimary,
        fontSize = 16.sp,
        overflow = TextOverflow.Ellipsis,
        maxLines = 1,
    )
}

/**
 * The caption text for a recent visit.
 *
 * @param count Number of recently visited items to display in the caption.
 */
@Composable
private fun RecentlyVisitedCaption(count: Int) {
    val stringId = if (count == 1) {
        R.string.history_search_group_site
    } else {
        R.string.history_search_group_sites
    }

    Text(
        text = String.format(LocalContext.current.getString(stringId), count),
        color = when (isSystemInDarkTheme()) {
            true -> FirefoxTheme.colors.textPrimary
            false -> FirefoxTheme.colors.textSecondary
        },
        fontSize = 12.sp,
        overflow = TextOverflow.Ellipsis,
        maxLines = 1
    )
}

/**
 * Menu shown for a [RecentlyVisitedItem].
 *
 * @see [DropdownMenu]
 *
 * @param showMenu Whether this is currently open and visible to the user.
 * @param menuItems List of options shown.
 * @param recentVisit The [RecentlyVisitedItem] for which this menu is shown.
 * @param onDismissRequest Called when the user chooses a menu option or requests to dismiss the menu.
 */
@Composable
private fun RecentlyVisitedMenu(
    showMenu: Boolean,
    menuItems: List<RecentVisitMenuItem>,
    recentVisit: RecentlyVisitedItem,
    onDismissRequest: () -> Unit,
) {
    DropdownMenu(
        expanded = showMenu,
        onDismissRequest = { onDismissRequest() },
        modifier = Modifier
            .background(color = FirefoxTheme.colors.layer2)
            .height(52.dp)
            .scrollable(
                state = ScrollState(0),
                orientation = Orientation.Vertical,
                enabled = false
            )
    ) {
        for (item in menuItems) {
            DropdownMenuItem(
                onClick = {
                    onDismissRequest()
                    item.onClick(recentVisit)
                },
                modifier = Modifier.fillMaxHeight()
            ) {
                Text(
                    text = item.title,
                    color = FirefoxTheme.colors.textPrimary,
                    maxLines = 1,
                    modifier = Modifier
                        .align(Alignment.Top)
                        .padding(top = 6.dp)
                        .scrollable(
                            state = ScrollState(0),
                            orientation = Orientation.Vertical,
                            enabled = false
                        )
                        .fillMaxHeight()
                )
            }
        }
    }
}

/**
 * A recent item divider.
 *
 * @param modifier [Modifier] allowing to perfectly place this.
 */
@Composable
private fun RecentlyVisitedDivider(
    modifier: Modifier = Modifier
) {
    Divider(
        modifier = modifier,
        color = FirefoxTheme.colors.borderDivider,
        thickness = 0.5.dp
    )
}

@ExperimentalFoundationApi
@Composable
@Preview
private fun RecentlyVisitedPreview() {
    FirefoxTheme {
        RecentlyVisited(
            recentVisits = listOf(
                RecentHistoryGroup(title = "running shoes"),
                RecentHistoryGroup(title = "mozilla"),
                RecentHistoryGroup(title = "firefox"),
                RecentHistoryGroup(title = "pocket")
            ),
            menuItems = emptyList()
        )
    }
}
