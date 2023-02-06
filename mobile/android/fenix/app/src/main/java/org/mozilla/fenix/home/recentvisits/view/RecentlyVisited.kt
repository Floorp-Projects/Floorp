/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.view

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.support.ktx.kotlin.trimmed
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.EagerFlingBehavior
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
 * @param recentVisits List of [RecentlyVisitedItem] to display.
 * @param menuItems List of [RecentVisitMenuItem] shown long clicking a [RecentlyVisitedItem].
 * @param backgroundColor The background [Color] of each item.
 * @param onRecentVisitClick Invoked when the user clicks on a recent visit.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun RecentlyVisited(
    recentVisits: List<RecentlyVisitedItem>,
    menuItems: List<RecentVisitMenuItem>,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    onRecentVisitClick: (RecentlyVisitedItem, Int) -> Unit = { _, _ -> },
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = backgroundColor,
        elevation = 6.dp,
    ) {
        val listState = rememberLazyListState()
        val flingBehavior = EagerFlingBehavior(lazyRowState = listState)

        LazyRow(
            modifier = Modifier.semantics {
                testTagsAsResourceId = true
                testTag = "recent.visits"
            },
            state = listState,
            contentPadding = PaddingValues(16.dp),
            horizontalArrangement = Arrangement.spacedBy(32.dp),
            flingBehavior = flingBehavior,
        ) {
            val itemsList = recentVisits.chunked(VISITS_PER_COLUMN)

            itemsIndexed(itemsList) { pageIndex, items ->
                Column(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    items.forEachIndexed { index, recentVisit ->
                        when (recentVisit) {
                            is RecentHistoryHighlight -> RecentlyVisitedHistoryHighlight(
                                recentVisit = recentVisit,
                                menuItems = menuItems,
                                clickableEnabled = listState.atLeastHalfVisibleItems.contains(pageIndex),
                                showDividerLine = index < items.size - 1,
                                onRecentVisitClick = {
                                    onRecentVisitClick(it, pageIndex + 1)
                                },
                            )
                            is RecentHistoryGroup -> RecentlyVisitedHistoryGroup(
                                recentVisit = recentVisit,
                                menuItems = menuItems,
                                clickableEnabled = listState.atLeastHalfVisibleItems.contains(pageIndex),
                                showDividerLine = index < items.size - 1,
                                onRecentVisitClick = {
                                    onRecentVisitClick(it, pageIndex + 1)
                                },
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
 * @param clickableEnabled Whether click actions should be invoked or not.
 * @param showDividerLine Whether to show a divider line at the bottom.
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
    clickableEnabled: Boolean,
    showDividerLine: Boolean,
    onRecentVisitClick: (RecentHistoryGroup) -> Unit = { _ -> },
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier
            .combinedClickable(
                enabled = clickableEnabled,
                onClick = { onRecentVisitClick(recentVisit) },
                onLongClick = { isMenuExpanded = true },
            )
            .size(268.dp, 56.dp)
            .semantics {
                testTagsAsResourceId = true
                testTag = "recent.visits.group"
            },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Image(
            painter = painterResource(R.drawable.ic_multiple_tabs),
            contentDescription = null,
            modifier = Modifier.size(24.dp),
        )

        Spacer(modifier = Modifier.width(16.dp))

        Column(
            modifier = Modifier.fillMaxSize(),
        ) {
            RecentlyVisitedTitle(
                text = recentVisit.title,
                modifier = Modifier
                    .padding(top = 7.dp, bottom = 2.dp)
                    .weight(1f)
                    .semantics {
                        testTagsAsResourceId = true
                        testTag = "recent.visits.group.title"
                    },
            )

            RecentlyVisitedCaption(
                count = recentVisit.historyMetadata.size,
                modifier = Modifier
                    .weight(1f)
                    .semantics {
                        testTagsAsResourceId = true
                        testTag = "recent.visits.group.caption"
                    },
            )

            if (showDividerLine) {
                Divider()
            }
        }

        RecentlyVisitedMenu(
            showMenu = isMenuExpanded,
            menuItems = menuItems,
            recentVisit = recentVisit,
            onDismissRequest = { isMenuExpanded = false },
        )
    }
}

/**
 * A recently visited history item.
 *
 * @param recentVisit The [RecentHistoryHighlight] to display.
 * @param menuItems List of [RecentVisitMenuItem] to display in a recent visit dropdown menu.
 * @param clickableEnabled Whether click actions should be invoked or not.
 * @param showDividerLine Whether to show a divider line at the bottom.
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
    clickableEnabled: Boolean,
    showDividerLine: Boolean,
    onRecentVisitClick: (RecentHistoryHighlight) -> Unit = { _ -> },
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier
            .combinedClickable(
                enabled = clickableEnabled,
                onClick = { onRecentVisitClick(recentVisit) },
                onLongClick = { isMenuExpanded = true },
            )
            .size(268.dp, 56.dp)
            .semantics {
                testTagsAsResourceId = true
                testTag = "recent.visits.highlight"
            },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Favicon(url = recentVisit.url, size = 24.dp)

        Spacer(modifier = Modifier.width(16.dp))

        Box(modifier = Modifier.fillMaxSize()) {
            RecentlyVisitedTitle(
                text = recentVisit.title.trimmed(),
                modifier = Modifier
                    .align(Alignment.CenterStart)
                    .semantics {
                        testTagsAsResourceId = true
                        testTag = "recent.visits.highlight.title"
                    },
            )

            if (showDividerLine) {
                Divider(modifier = Modifier.align(Alignment.BottomCenter))
            }
        }

        RecentlyVisitedMenu(
            showMenu = isMenuExpanded,
            menuItems = menuItems,
            recentVisit = recentVisit,
            onDismissRequest = { isMenuExpanded = false },
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
    modifier: Modifier = Modifier,
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
 * @param modifier [Modifier] allowing to perfectly place this.
 */
@Composable
private fun RecentlyVisitedCaption(
    count: Int,
    modifier: Modifier,
) {
    val stringId = if (count == 1) {
        R.string.history_search_group_site_1
    } else {
        R.string.history_search_group_sites_1
    }

    Text(
        text = String.format(LocalContext.current.getString(stringId), count),
        modifier = modifier,
        color = when (isSystemInDarkTheme()) {
            true -> FirefoxTheme.colors.textPrimary
            false -> FirefoxTheme.colors.textSecondary
        },
        fontSize = 12.sp,
        overflow = TextOverflow.Ellipsis,
        maxLines = 1,
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
@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun RecentlyVisitedMenu(
    showMenu: Boolean,
    menuItems: List<RecentVisitMenuItem>,
    recentVisit: RecentlyVisitedItem,
    onDismissRequest: () -> Unit,
) {
    DisposableEffect(LocalConfiguration.current.orientation) {
        onDispose { onDismissRequest() }
    }

    DropdownMenu(
        expanded = showMenu,
        onDismissRequest = { onDismissRequest() },
        modifier = Modifier
            .background(color = FirefoxTheme.colors.layer2)
            .semantics {
                testTagsAsResourceId = true
                testTag = "recent.visit.menu"
            },
    ) {
        for (item in menuItems) {
            DropdownMenuItem(
                onClick = {
                    onDismissRequest()
                    item.onClick(recentVisit)
                },
            ) {
                Text(
                    text = item.title,
                    color = FirefoxTheme.colors.textPrimary,
                    maxLines = 1,
                    modifier = Modifier
                        .fillMaxHeight()
                        .align(Alignment.CenterVertically),
                )
            }
        }
    }
}

/**
 * Get the indexes in list of all items which have more than half showing.
 */
private val LazyListState.atLeastHalfVisibleItems
    get() = layoutInfo
        .visibleItemsInfo
        .filter {
            val startEdge = maxOf(0, layoutInfo.viewportStartOffset - it.offset)
            val endEdge = maxOf(0, it.offset + it.size - layoutInfo.viewportEndOffset)
            return@filter startEdge + endEdge < it.size / 2
        }.map { it.index }

@Composable
@Preview
private fun RecentlyVisitedPreview() {
    FirefoxTheme {
        RecentlyVisited(
            recentVisits = listOf(
                RecentHistoryGroup(title = "running shoes"),
                RecentHistoryGroup(title = "mozilla"),
                RecentHistoryGroup(title = "firefox"),
                RecentHistoryGroup(title = "pocket"),
            ),
            menuItems = emptyList(),
        )
    }
}
