/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.view

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowColumn
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.support.ktx.kotlin.trimmed
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.ext.thenConditional
import org.mozilla.fenix.compose.list.FaviconListItem
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryGroup
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem.RecentHistoryHighlight
import org.mozilla.fenix.theme.FirefoxTheme

// Number of recently visited items per column.
private const val VISITS_PER_COLUMN = 3

private val recentlyVisitedItemMaxWidth = 320.dp

private val horizontalArrangementSpacing = 32.dp
private val contentPadding = 16.dp

/**
 * A list of recently visited items.
 *
 * @param recentVisits List of [RecentlyVisitedItem] to display.
 * @param menuItems List of [RecentVisitMenuItem] shown long clicking a [RecentlyVisitedItem].
 * @param backgroundColor The background [Color] of each item.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit. The first parameter is
 * the [RecentlyVisitedItem] that was clicked and the second parameter is the "page" or column number
 * the item resides in.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun RecentlyVisited(
    recentVisits: List<RecentlyVisitedItem>,
    menuItems: List<RecentVisitMenuItem>,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    onRecentVisitClick: (RecentlyVisitedItem, pageNumber: Int) -> Unit = { _, _ -> },
) {
    val isSingleColumn by remember(recentVisits) { derivedStateOf { recentVisits.size <= VISITS_PER_COLUMN } }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .thenConditional(
                modifier = Modifier.horizontalScroll(state = rememberScrollState()),
                predicate = { !isSingleColumn },
            )
            .padding(
                horizontal = contentPadding,
                vertical = 8.dp,
            ),
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(8.dp),
            backgroundColor = backgroundColor,
            elevation = 6.dp,
        ) {
            FlowColumn(
                modifier = Modifier.fillMaxWidth(),
                maxItemsInEachColumn = VISITS_PER_COLUMN,
                horizontalArrangement = Arrangement.spacedBy(horizontalArrangementSpacing),
            ) {
                recentVisits.forEachIndexed { index, recentVisit ->
                    // Don't display the divider when its the last item in a column or the last item
                    // in the table.
                    val showDivider = (index + 1) % VISITS_PER_COLUMN != 0 &&
                        index != recentVisits.lastIndex
                    val pageIndex = index / VISITS_PER_COLUMN
                    val pageNumber = pageIndex + 1

                    Box(
                        modifier = if (isSingleColumn) {
                            Modifier.fillMaxWidth()
                        } else {
                            Modifier.widthIn(max = recentlyVisitedItemMaxWidth)
                        },
                    ) {
                        when (recentVisit) {
                            is RecentHistoryHighlight -> RecentlyVisitedHistoryHighlight(
                                recentVisit = recentVisit,
                                menuItems = menuItems,
                                onRecentVisitClick = {
                                    onRecentVisitClick(it, pageNumber)
                                },
                            )

                            is RecentHistoryGroup -> RecentlyVisitedHistoryGroup(
                                recentVisit = recentVisit,
                                menuItems = menuItems,
                                onRecentVisitClick = {
                                    onRecentVisitClick(it, pageNumber)
                                },
                            )
                        }

                        if (showDivider) {
                            Divider(
                                modifier = Modifier
                                    .align(Alignment.BottomCenter)
                                    .padding(horizontal = contentPadding),
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
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@OptIn(
    ExperimentalFoundationApi::class,
    ExperimentalComposeUiApi::class,
)
@Composable
private fun RecentlyVisitedHistoryGroup(
    recentVisit: RecentHistoryGroup,
    menuItems: List<RecentVisitMenuItem>,
    onRecentVisitClick: (RecentHistoryGroup) -> Unit = { _ -> },
) {
    var isMenuExpanded by remember { mutableStateOf(false) }
    val captionId = if (recentVisit.historyMetadata.size == 1) {
        R.string.history_search_group_site_1
    } else {
        R.string.history_search_group_sites_1
    }

    Box {
        IconListItem(
            label = recentVisit.title.trimmed(),
            modifier = Modifier
                .combinedClickable(
                    onClick = { onRecentVisitClick(recentVisit) },
                    onLongClick = { isMenuExpanded = true },
                ),
            beforeIconPainter = painterResource(R.drawable.ic_multiple_tabs),
            description = stringResource(id = captionId, recentVisit.historyMetadata.size),
        )

        ContextualMenu(
            showMenu = isMenuExpanded,
            onDismissRequest = { isMenuExpanded = false },
            menuItems = menuItems.map { item -> MenuItem(item.title) { item.onClick(recentVisit) } },
            modifier = Modifier.semantics {
                testTagsAsResourceId = true
                testTag = "recent.visit.menu"
            },
        )
    }
}

/**
 * A recently visited history item.
 *
 * @param recentVisit The [RecentHistoryHighlight] to display.
 * @param menuItems List of [RecentVisitMenuItem] to display in a recent visit dropdown menu.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@OptIn(
    ExperimentalFoundationApi::class,
    ExperimentalComposeUiApi::class,
)
@Composable
private fun RecentlyVisitedHistoryHighlight(
    recentVisit: RecentHistoryHighlight,
    menuItems: List<RecentVisitMenuItem>,
    onRecentVisitClick: (RecentHistoryHighlight) -> Unit = { _ -> },
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Box {
        FaviconListItem(
            label = recentVisit.title.trimmed(),
            url = recentVisit.url,
            modifier = Modifier
                .combinedClickable(
                    onClick = { onRecentVisitClick(recentVisit) },
                    onLongClick = { isMenuExpanded = true },
                ),
        )

        ContextualMenu(
            showMenu = isMenuExpanded,
            onDismissRequest = { isMenuExpanded = false },
            menuItems = menuItems.map { item -> MenuItem(item.title) { item.onClick(recentVisit) } },
            modifier = Modifier.semantics {
                testTagsAsResourceId = true
                testTag = "recent.visit.menu"
            },
        )
    }
}

@Composable
@LightDarkPreview
private fun RecentlyVisitedMultipleColumnsPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(vertical = contentPadding),
        ) {
            RecentlyVisited(
                recentVisits = listOf(
                    RecentHistoryGroup(title = "running shoes"),
                    RecentHistoryGroup(title = "mozilla"),
                    RecentHistoryGroup(title = "firefox"),
                    RecentHistoryGroup(title = "pocket"),
                    RecentHistoryHighlight(title = "Mozilla", url = "www.mozilla.com"),
                ),
                menuItems = emptyList(),
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun RecentlyVisitedSingleColumnPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(vertical = contentPadding),
        ) {
            RecentlyVisited(
                recentVisits = listOf(
                    RecentHistoryGroup(title = "running shoes"),
                    RecentHistoryHighlight(title = "Mozilla", url = "www.mozilla.com"),
                ),
                menuItems = emptyList(),
            )
        }
    }
}

@Composable
@Preview(widthDp = 250)
private fun RecentlyVisitedSingleColumnSmallPreview() {
    RecentlyVisitedSingleColumnPreview()
}
