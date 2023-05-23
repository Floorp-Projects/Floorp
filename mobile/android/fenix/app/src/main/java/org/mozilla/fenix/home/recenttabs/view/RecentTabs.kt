/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber", "TooManyFunctions")

package org.mozilla.fenix.home.recenttabs.view

import android.graphics.Bitmap
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.icons.compose.Loader
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.ktx.kotlin.trimmed
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.Image
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.ThumbnailCard
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A list of recent tabs to jump back to.
 *
 * @param recentTabs List of [RecentTab] to display.
 * @param menuItems List of [RecentTabMenuItem] shown long clicking a [RecentTab].
 * @param backgroundColor The background [Color] of each item.
 * @param onRecentTabClick Invoked when the user clicks on a recent tab.
 */
@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun RecentTabs(
    recentTabs: List<RecentTab>,
    menuItems: List<RecentTabMenuItem>,
    backgroundColor: Color = FirefoxTheme.colors.layer2,
    onRecentTabClick: (String) -> Unit = {},
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                testTagsAsResourceId = true
                testTag = "recent.tabs"
            },
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        recentTabs.forEach { tab ->
            when (tab) {
                is RecentTab.Tab -> {
                    RecentTabItem(
                        tab = tab,
                        menuItems = menuItems,
                        backgroundColor = backgroundColor,
                        onRecentTabClick = onRecentTabClick,
                    )
                }
            }
        }
    }
}

/**
 * A recent tab item.
 *
 * @param tab [RecentTab.Tab] that was recently viewed.
 * @param backgroundColor The background [Color] of the item.
 * @param onRecentTabClick Invoked when the user clicks on a recent tab.
 */
@OptIn(
    ExperimentalFoundationApi::class,
    ExperimentalComposeUiApi::class,
)
@Composable
@Suppress("LongMethod")
private fun RecentTabItem(
    tab: RecentTab.Tab,
    menuItems: List<RecentTabMenuItem>,
    backgroundColor: Color,
    onRecentTabClick: (String) -> Unit = {},
) {
    var isMenuExpanded by remember { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(112.dp)
            .combinedClickable(
                enabled = true,
                onClick = { onRecentTabClick(tab.state.id) },
                onLongClick = { isMenuExpanded = true },
            ),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = backgroundColor,
        elevation = 6.dp,
    ) {
        Row(
            modifier = Modifier.padding(16.dp),
        ) {
            RecentTabImage(
                tab = tab,
                modifier = Modifier
                    .size(108.dp, 80.dp)
                    .clip(RoundedCornerShape(8.dp)),
                contentScale = ContentScale.Crop,
            )

            Spacer(modifier = Modifier.width(16.dp))

            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(
                    text = tab.state.content.title.ifEmpty { tab.state.content.url.trimmed() },
                    modifier = Modifier.semantics {
                        testTagsAsResourceId = true
                        testTag = "recent.tab.title"
                    },
                    color = FirefoxTheme.colors.textPrimary,
                    fontSize = 14.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )

                Row {
                    RecentTabIcon(
                        url = tab.state.content.url,
                        modifier = Modifier
                            .size(18.dp)
                            .clip(RoundedCornerShape(2.dp)),
                        contentScale = ContentScale.Crop,
                        icon = tab.state.content.icon,
                    )

                    Spacer(modifier = Modifier.width(8.dp))

                    Text(
                        text = tab.state.content.url.trimmed(),
                        modifier = Modifier.semantics {
                            testTagsAsResourceId = true
                            testTag = "recent.tab.url"
                        },
                        color = FirefoxTheme.colors.textSecondary,
                        fontSize = 12.sp,
                        overflow = TextOverflow.Ellipsis,
                        maxLines = 1,
                    )
                }
            }

            ContextualMenu(
                showMenu = isMenuExpanded,
                onDismissRequest = { isMenuExpanded = false },
                menuItems = menuItems.map { item -> MenuItem(item.title) { item.onClick(tab) } },
                modifier = Modifier.semantics {
                    testTagsAsResourceId = true
                    testTag = "recent.tab.menu"
                },
            )
        }
    }
}

/**
 * A recent tab image.
 *
 * @param tab [RecentTab] that was recently viewed.
 * @param modifier [Modifier] used to draw the image content.
 * @param contentScale [ContentScale] used to draw image content.
 */
@Composable
fun RecentTabImage(
    tab: RecentTab.Tab,
    modifier: Modifier = Modifier,
    contentScale: ContentScale = ContentScale.FillWidth,
) {
    val previewImageUrl = tab.state.content.previewImageUrl

    when {
        !previewImageUrl.isNullOrEmpty() -> {
            Image(
                url = previewImageUrl,
                modifier = modifier,
                targetSize = 108.dp,
                contentScale = ContentScale.Crop,
            )
        }
        else -> ThumbnailCard(
            url = tab.state.content.url,
            key = tab.state.id,
            modifier = modifier,
            contentScale = contentScale,
        )
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
    icon: Bitmap? = null,
) {
    when {
        icon != null -> {
            Image(
                painter = BitmapPainter(icon.asImageBitmap()),
                contentDescription = null,
                modifier = modifier,
                contentScale = contentScale,
                alignment = alignment,
            )
        }
        !inComposePreview -> {
            components.core.icons.Loader(url) {
                Placeholder {
                    PlaceHolderTabIcon(modifier)
                }

                WithIcon { icon ->
                    Image(
                        painter = icon.painter,
                        contentDescription = null,
                        modifier = modifier,
                        contentScale = contentScale,
                    )
                }
            }
        }
        else -> {
            PlaceHolderTabIcon(modifier)
        }
    }
}

/**
 * A placeholder for the recent tab icon.
 *
 * @param modifier [Modifier] used to shape the content.
 */
@Composable
private fun PlaceHolderTabIcon(modifier: Modifier) {
    Box(
        modifier = modifier.background(
            color = when (isSystemInDarkTheme()) {
                true -> PhotonColors.DarkGrey60
                false -> PhotonColors.LightGrey30
            },
        ),
    )
}

@LightDarkPreview
@Composable
private fun RecentTabsPreview() {
    val tab = RecentTab.Tab(
        TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        ),
    )

    FirefoxTheme {
        RecentTabs(
            recentTabs = listOf(
                tab,
            ),
            menuItems = listOf(
                RecentTabMenuItem(
                    title = "Menu item",
                    onClick = {},
                ),
            ),
            onRecentTabClick = {},
        )
    }
}
