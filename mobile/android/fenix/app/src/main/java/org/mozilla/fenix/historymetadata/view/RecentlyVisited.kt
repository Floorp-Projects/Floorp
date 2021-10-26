/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.scrollable
import androidx.compose.foundation.layout.Arrangement
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
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.theme.FirefoxTheme

// Number of recently visited items per column.
private const val VISITS_PER_COLUMN = 3

/**
 * A list of recently visited items.
 *
 * @param recentVisits List of [HistoryMetadataGroup] to display.
 * @param menuItems List of [RecentVisitMenuItem] to display in a recent visit dropdown menu.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@Composable
fun RecentlyVisited(
    recentVisits: List<HistoryMetadataGroup>,
    menuItems: List<RecentVisitMenuItem>,
    onRecentVisitClick: (HistoryMetadataGroup, Int) -> Unit = { _, _ -> }
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.surface,
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
                        RecentVisitItem(
                            recentVisit = recentVisit,
                            menuItems = menuItems,
                            showDividerLine = index < items.size - 1,
                            onRecentVisitClick = onRecentVisitClick,
                            pageNumber = pageIndex + 1
                        )
                    }
                }
            }
        }
    }
}

/**
 * A recent visit item.
 *
 * @param recentVisit The [HistoryMetadataGroup] to display.
 * @param menuItems List of [RecentVisitMenuItem] to display in a recent visit dropdown menu.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 * @param pageNumber which page is the item on.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentVisitItem(
    recentVisit: HistoryMetadataGroup,
    menuItems: List<RecentVisitMenuItem>,
    showDividerLine: Boolean,
    onRecentVisitClick: (HistoryMetadataGroup, Int) -> Unit = { _, _ -> },
    pageNumber: Int
) {
    var menuExpanded by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier
            .combinedClickable(
                onClick = { onRecentVisitClick(recentVisit, pageNumber) },
                onLongClick = { menuExpanded = true }
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
            Text(
                text = recentVisit.title,
                modifier = Modifier.padding(top = 7.dp, bottom = 2.dp),
                color = FirefoxTheme.colors.textPrimary,
                fontSize = 16.sp,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1
            )

            RecentlyVisitedCaption(recentVisit.historyMetadata.size)

            if (showDividerLine) {
                Divider(
                    modifier = Modifier.padding(top = 9.dp),
                    color = FirefoxTheme.colors.dividerLine,
                    thickness = 0.5.dp
                )
            }
        }

        DropdownMenu(
            expanded = menuExpanded,
            onDismissRequest = { menuExpanded = false },
            modifier = Modifier.background(color = FirefoxTheme.colors.surface)
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
                        menuExpanded = false
                        item.onClick(recentVisit)
                    },
                    modifier = Modifier.fillMaxHeight()
                ) {
                    Text(
                        text = item.title,
                        color = FirefoxTheme.colors.textPrimary,
                        maxLines = 1,
                        modifier = Modifier.align(Alignment.Top)
                            .padding(top = 6.dp)
                            .scrollable(
                                state = ScrollState(0),
                                orientation = Orientation.Vertical,
                                enabled = false
                            ).fillMaxHeight()
                    )
                }
            }
        }
    }
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
        color = FirefoxTheme.colors.textSecondary,
        fontSize = 12.sp,
        overflow = TextOverflow.Ellipsis,
        maxLines = 1
    )
}

@ExperimentalFoundationApi
@Composable
@Preview
private fun RecentlyVisitedPreview() {
    FirefoxTheme {
        RecentlyVisited(
            recentVisits = listOf(
                HistoryMetadataGroup(title = "running shoes"),
                HistoryMetadataGroup(title = "mozilla"),
                HistoryMetadataGroup(title = "firefox"),
                HistoryMetadataGroup(title = "pocket")
            ),
            menuItems = emptyList()
        )
    }
}
