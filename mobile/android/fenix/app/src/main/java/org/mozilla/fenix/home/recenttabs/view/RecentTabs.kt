/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.home.recenttabs.view

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.icons.compose.Loader
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.support.ktx.kotlin.getRepresentativeSnippet
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.Image
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A list of recent tabs to jump back to.
 *
 * @param recentTabs List of [RecentTab] to display.
 * @param onRecentTabClick Invoked when the user clicks on a recent tab.
 * @param onRecentSearchGroupClicked Invoked when the user clicks on a recent search group.
 */
@Composable
fun RecentTabs(
    recentTabs: List<RecentTab>,
    onRecentTabClick: (String) -> Unit = {},
    onRecentSearchGroupClicked: (String) -> Unit = {}
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
                        onRecentTabClick = onRecentTabClick
                    )
                }
                is RecentTab.SearchGroup -> {
                    if (components.settings.searchTermTabGroupsAreEnabled) {
                        RecentSearchGroupItem(
                            searchTerm = tab.searchTerm,
                            tabId = tab.tabId,
                            count = tab.count,
                            onSearchGroupClicked = onRecentSearchGroupClicked
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
@Suppress("LongParameterList")
@Composable
private fun RecentTabItem(
    tab: RecentTab.Tab,
    onRecentTabClick: (String) -> Unit = {}
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(112.dp)
            .clickable { onRecentTabClick(tab.state.id) },
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.surface,
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
                RecentTabTitle(title = tab.state.content.title.ifEmpty { tab.state.content.url })

                Row {
                    RecentTabIcon(
                        url = tab.state.content.url,
                        modifier = Modifier
                            .size(18.dp, 18.dp)
                            .clip(RoundedCornerShape(2.dp)),
                        icon = tab.state.content.icon
                    )

                    Spacer(modifier = Modifier.width(8.dp))

                    RecentTabSubtitle(subtitle = tab.state.content.url)
                }
            }
        }
    }
}

/**
 * A recent search group item.
 *
 * @param searchTerm The search term for the group.
 * @param tabId The id of the last accessed tab in the group.
 * @param count Count of how many tabs belongs to the group.
 * @param onSearchGroupClicked Invoked when the user clicks on a group.
 */
@Suppress("LongParameterList")
@Composable
private fun RecentSearchGroupItem(
    searchTerm: String,
    tabId: String,
    count: Int,
    onSearchGroupClicked: (String) -> Unit = {}
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(112.dp)
            .clickable { onSearchGroupClicked(tabId) },
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.surface,
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
                RecentTabTitle(title = stringResource(R.string.recent_tabs_search_term, searchTerm))

                Row {
                    Icon(
                        painter = painterResource(id = R.drawable.ic_all_tabs),
                        modifier = Modifier.size(18.dp),
                        contentDescription = null,
                        tint = FirefoxTheme.colors.textSecondary
                    )

                    Spacer(modifier = Modifier.width(8.dp))

                    RecentTabSubtitle(subtitle = stringResource(R.string.recent_tabs_search_term_count, count))
                }
            }
        }
    }
}

/**
 * A recent tab image.
 *
 * @param tab [RecentTab.Tab] that was recently viewed.
 * @param modifier [Modifier] used to draw the image content.
 * @param contentScale [ContentScale] used to draw image content.
 * @param alignment [Alignment] used to draw the image content.
 * is null.
 */
@Composable
@Suppress("LongParameterList")
private fun RecentTabImage(
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
        else -> {
            Card(
                modifier = modifier,
                backgroundColor = colorResource(id = R.color.photonGrey20)
            ) {
                components.core.icons.Loader(tab.state.content.url) {
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
                        Box(
                            modifier = Modifier.size(36.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Image(
                                painter = icon.painter,
                                contentDescription = null,
                                modifier = Modifier
                                    .size(36.dp)
                                    .clip(RoundedCornerShape(8.dp)),
                                contentScale = ContentScale.Fit
                            )
                        }
                    }
                }

                ThumbnailImage(
                    tabId = tab.state.id,
                    modifier = modifier,
                    contentScale = contentScale,
                    alignment = alignment
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

/**
 * A recent tab title.
 *
 * @param title The title of the tab.
 */
@Composable
private fun RecentTabTitle(title: String) {
    Text(
        text = title,
        color = FirefoxTheme.colors.textPrimary,
        fontSize = 14.sp,
        overflow = TextOverflow.Ellipsis,
        maxLines = 2
    )
}

/**
 * A recent tab subtitle.
 *
 * @param subtitle The loaded URL of the tab.
 */
@Composable
private fun RecentTabSubtitle(subtitle: String) {
    Text(
        text = subtitle.getRepresentativeSnippet(),
        color = FirefoxTheme.colors.textSecondary,
        fontSize = 12.sp,
        overflow = TextOverflow.Ellipsis,
        maxLines = 1
    )
}

@Composable
private fun ThumbnailImage(
    tabId: String,
    modifier: Modifier,
    contentScale: ContentScale,
    alignment: Alignment
) {
    val rememberBitmap = remember(tabId) { mutableStateOf<ImageBitmap?>(null) }
    val size = LocalDensity.current.run { 108.dp.toPx().toInt() }
    val request = ImageLoadRequest(tabId, size)
    val storage = components.core.thumbnailStorage
    val bitmap = rememberBitmap.value

    LaunchedEffect(tabId) {
        rememberBitmap.value = storage.loadThumbnail(request).await()?.asImageBitmap()
    }

    if (bitmap != null) {
        val painter = BitmapPainter(bitmap)
        Image(
            painter = painter,
            contentDescription = null,
            modifier = modifier,
            contentScale = contentScale,
            alignment = alignment
        )
    }
}
