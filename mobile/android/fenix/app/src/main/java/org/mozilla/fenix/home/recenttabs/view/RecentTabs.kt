/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber", "TooManyFunctions")

package org.mozilla.fenix.home.recenttabs.view

import android.graphics.Bitmap
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.icons.compose.Loader
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.Image
import org.mozilla.fenix.compose.ThumbnailCard
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A list of recent tabs to jump back to.
 *
 * @param recentTabs List of [RecentTab] to display.
 * @param menuItems List of [RecentTabMenuItem] shown long clicking a [RecentTab].
 * @param onRecentTabClick Invoked when the user clicks on a recent tab.
 * @param onRecentSearchGroupClick Invoked when the user clicks on a recent search group.
 */
@Composable
fun RecentTabs(
    recentTabs: List<RecentTab>,
    menuItems: List<RecentTabMenuItem>,
    onRecentTabClick: (String) -> Unit = {},
    onRecentSearchGroupClick: (String) -> Unit = {},
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        recentTabs.forEach { tab ->
            when (tab) {
                is RecentTab.Tab -> {
                    RecentTabItem(
                        tab = tab,
                        menuItems = menuItems,
                        onRecentTabClick = onRecentTabClick
                    )
                }
                is RecentTab.SearchGroup -> {
                    if (components.settings.searchTermTabGroupsAreEnabled) {
                        RecentSearchGroupItem(
                            searchTerm = tab.searchTerm,
                            tabId = tab.tabId,
                            count = tab.count,
                            onSearchGroupClick = onRecentSearchGroupClick
                        )
                    }
                }
            }
        }
    }
}

/**
 * A recent tab item.
 *
 * @param tab [RecentTab.Tab] that was recently viewed.
 * @param onRecentTabClick Invoked when the user clicks on a recent tab.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun RecentTabItem(
    tab: RecentTab.Tab,
    menuItems: List<RecentTabMenuItem>,
    onRecentTabClick: (String) -> Unit = {}
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(112.dp)
            .combinedClickable(
                enabled = true,
                onClick = { onRecentTabClick(tab.state.id) },
                onLongClick = { isMenuExpanded = true }
            ),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.layer2,
        elevation = 6.dp
    ) {
        Row(
            modifier = Modifier.padding(16.dp)
        ) {
            RecentTabImage(
                tab = tab,
                modifier = Modifier
                    .size(108.dp, 80.dp)
                    .clip(RoundedCornerShape(8.dp))
            )

            Spacer(modifier = Modifier.width(16.dp))

            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = tab.state.content.title.ifEmpty { tab.state.content.url },
                    color = FirefoxTheme.colors.textPrimary,
                    fontSize = 14.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )

                Row {
                    RecentTabIcon(
                        url = tab.state.content.url,
                        modifier = Modifier.size(18.dp).clip(RoundedCornerShape(2.dp)),
                        icon = tab.state.content.icon
                    )

                    Spacer(modifier = Modifier.width(8.dp))

                    Text(
                        text = tab.state.content.url,
                        color = FirefoxTheme.colors.textSecondary,
                        fontSize = 12.sp,
                        overflow = TextOverflow.Ellipsis,
                        maxLines = 1,
                    )
                }
            }

            RecentTabMenu(
                showMenu = isMenuExpanded,
                menuItems = menuItems,
                tab = tab,
                onDismissRequest = { isMenuExpanded = false }
            )
        }
    }
}

/**
 * A recent search group item.
 *
 * @param searchTerm The search term for the group.
 * @param tabId The id of the last accessed tab in the group.
 * @param count Count of how many tabs belongs to the group.
 * @param onSearchGroupClick Invoked when the user clicks on a group.
 */
@Composable
private fun RecentSearchGroupItem(
    searchTerm: String,
    tabId: String,
    count: Int,
    onSearchGroupClick: (String) -> Unit = {}
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(112.dp)
            .clickable { onSearchGroupClick(tabId) },
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.layer2,
        elevation = 6.dp
    ) {
        Row(
            modifier = Modifier.padding(16.dp)
        ) {
            Image(
                painter = painterResource(id = R.drawable.ic_search_group_thumbnail),
                contentDescription = null,
                modifier = Modifier.size(108.dp, 80.dp),
                contentScale = ContentScale.FillWidth,
                alignment = Alignment.Center
            )

            Spacer(modifier = Modifier.width(16.dp))

            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = stringResource(R.string.recent_tabs_search_term, searchTerm),
                    color = FirefoxTheme.colors.textPrimary,
                    fontSize = 14.sp,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 2,
                )

                Row {
                    Icon(
                        painter = painterResource(id = R.drawable.ic_all_tabs),
                        modifier = Modifier.size(18.dp),
                        contentDescription = null,
                        tint = when (isSystemInDarkTheme()) {
                            true -> FirefoxTheme.colors.textPrimary
                            false -> FirefoxTheme.colors.textSecondary
                        }
                    )

                    Spacer(modifier = Modifier.width(8.dp))

                    Text(
                        text = stringResource(R.string.recent_tabs_search_term_count_2, count),
                        color = FirefoxTheme.colors.textSecondary,
                        fontSize = 12.sp,
                        overflow = TextOverflow.Ellipsis,
                        maxLines = 1,
                    )
                }
            }
        }
    }
}

/**
 * A recent tab image.
 *
 * @param tab [RecentTab] that was recently viewed.
 * @param modifier [Modifier] used to draw the image content.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 */
@Composable
fun RecentTabImage(
    tab: RecentTab.Tab,
    modifier: Modifier = Modifier,
    contentScale: ContentScale = ContentScale.FillWidth,
    alignment: Alignment = Alignment.TopCenter
) {
    val previewImageUrl = tab.state.content.previewImageUrl
    val thumbnail = tab.state.content.thumbnail

    when {
        !previewImageUrl.isNullOrEmpty() -> {
            Image(
                url = previewImageUrl,
                modifier = modifier,
                targetSize = 108.dp,
                contentScale = ContentScale.Crop
            )
        }
        thumbnail != null -> {
            Image(
                painter = BitmapPainter(thumbnail.asImageBitmap()),
                contentDescription = null,
                modifier = modifier,
                contentScale = contentScale,
                alignment = alignment
            )
        }
        else -> ThumbnailCard(
            url = tab.state.content.url,
            key = tab.state.id,
            modifier = modifier
        )
    }
}

/**
 * Menu shown for a [RecentTab.Tab].
 *
 * @see [DropdownMenu]
 *
 * @param showMenu Whether this is currently open and visible to the user.
 * @param menuItems List of options shown.
 * @param tab The [RecentTab.Tab] for which this menu is shown.
 * @param onDismissRequest Called when the user chooses a menu option or requests to dismiss the menu.
 */
@Composable
private fun RecentTabMenu(
    showMenu: Boolean,
    menuItems: List<RecentTabMenuItem>,
    tab: RecentTab.Tab,
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
    ) {
        for (item in menuItems) {
            DropdownMenuItem(
                onClick = {
                    onDismissRequest()
                    item.onClick(tab)
                },
            ) {
                Text(
                    text = item.title,
                    color = FirefoxTheme.colors.textPrimary,
                    maxLines = 1,
                    modifier = Modifier
                        .fillMaxHeight()
                        .align(Alignment.CenterVertically)
                )
            }
        }
    }
}

/**
 * A recent tab icon.
 *
 * @param url The loaded URL of the tab.
 * @param modifier [Modifier] used to draw the image content.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 * @param icon The icon of the tab. Fallback to loading the icon from the [url] if the [icon]
 * is null.
 */
@Composable
private fun RecentTabIcon(
    url: String,
    modifier: Modifier = Modifier,
    contentScale: ContentScale = ContentScale.Fit,
    alignment: Alignment = Alignment.Center,
    icon: Bitmap? = null
) {
    when {
        icon != null -> {
            Image(
                painter = BitmapPainter(icon.asImageBitmap()),
                contentDescription = null,
                modifier = modifier,
                contentScale = contentScale,
                alignment = alignment
            )
        }
        else -> {
            components.core.icons.Loader(url) {
                Placeholder {
                    Box(
                        modifier = Modifier.background(
                            color = when (isSystemInDarkTheme()) {
                                true -> PhotonColors.DarkGrey30
                                false -> PhotonColors.LightGrey30
                            }
                        )
                    )
                }

                WithIcon { icon ->
                    Image(
                        painter = icon.painter,
                        contentDescription = null,
                        modifier = modifier,
                        contentScale = ContentScale.Fit
                    )
                }
            }
        }
    }
}
