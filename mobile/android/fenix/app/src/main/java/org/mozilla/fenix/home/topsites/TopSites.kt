/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ContextualMenu
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.PagerIndicator
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.ext.bitmapForUrl
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.ceil

private const val TOP_SITES_PER_PAGE = 8
private const val TOP_SITES_PER_ROW = 4
private const val TOP_SITES_ITEM_SIZE = 84
private const val TOP_SITES_ROW_WIDTH = TOP_SITES_PER_ROW * TOP_SITES_ITEM_SIZE
private const val TOP_SITES_FAVICON_CARD_SIZE = 60
private const val TOP_SITES_FAVICON_SIZE = 36

/**
 * A list of top sites.
 *
 * @param topSites List of [TopSite] to display.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
@Suppress("LongParameterList")
fun TopSites(
    topSites: List<TopSite>,
    onTopSiteClick: (TopSite) -> Unit,
    onTopSiteLongClick: (TopSite) -> Unit,
    onOpenInPrivateTabClicked: (topSite: TopSite) -> Unit,
    onRenameTopSiteClicked: (topSite: TopSite) -> Unit,
    onRemoveTopSiteClicked: (topSite: TopSite) -> Unit,
    onSettingsClicked: () -> Unit,
    onSponsorPrivacyClicked: () -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        val pagerState = rememberPagerState()
        val pageCount = ceil((topSites.size.toDouble() / TOP_SITES_PER_PAGE)).toInt()

        Box(
            modifier = Modifier.fillMaxWidth(),
            contentAlignment = Alignment.Center,
        ) {
            HorizontalPager(
                pageCount = pageCount,
                state = pagerState,
            ) { page ->
                Column {
                    val topSitesWindows = topSites.windowed(
                        size = TOP_SITES_PER_PAGE,
                        step = TOP_SITES_PER_PAGE,
                        partialWindows = true,
                    )[page].chunked(TOP_SITES_PER_ROW)

                    for (items in topSitesWindows) {
                        Row(modifier = Modifier.defaultMinSize(minWidth = TOP_SITES_ROW_WIDTH.dp)) {
                            items.forEach { topSite ->
                                TopSiteItem(
                                    topSite = topSite,
                                    menuItems = getMenuItems(
                                        topSite = topSite,
                                        onOpenInPrivateTabClicked = onOpenInPrivateTabClicked,
                                        onRenameTopSiteClicked = onRenameTopSiteClicked,
                                        onRemoveTopSiteClicked = onRemoveTopSiteClicked,
                                        onSettingsClicked = onSettingsClicked,
                                        onSponsorPrivacyClicked = onSponsorPrivacyClicked,
                                    ),
                                    onTopSiteClick = { item -> onTopSiteClick(item) },
                                    onTopSiteLongClick = onTopSiteLongClick,
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(12.dp))
                    }
                }
            }
        }

        if (pageCount > 1) {
            Spacer(modifier = Modifier.height(8.dp))

            PagerIndicator(
                pagerState = pagerState,
                pageCount = pageCount,
                modifier = Modifier.padding(horizontal = 16.dp),
                spacing = 4.dp,
            )
        }
    }
}

/**
 * A top site item.
 *
 * @param topSite The [TopSite] to display.
 * @param menuItems List of [MenuItem]s to display in a top site dropdown menu.
 * @param onTopSiteClick Invoked when the user clicks on a top site.
 * @param onTopSiteLongClick Invoked when the user long clicks on a top site.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun TopSiteItem(
    topSite: TopSite,
    menuItems: List<MenuItem>,
    onTopSiteClick: (TopSite) -> Unit,
    onTopSiteLongClick: (TopSite) -> Unit,
) {
    var menuExpanded by remember { mutableStateOf(false) }

    Box {
        Column(
            modifier = Modifier
                .combinedClickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null,
                    onClick = { onTopSiteClick(topSite) },
                    onLongClick = {
                        onTopSiteLongClick(topSite)
                        menuExpanded = true
                    },
                )
                .width(TOP_SITES_ITEM_SIZE.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Spacer(modifier = Modifier.height(4.dp))

            TopSiteFaviconCard(topSite = topSite)

            Spacer(modifier = Modifier.height(6.dp))

            Row(
                modifier = Modifier.width(TOP_SITES_ITEM_SIZE.dp),
                horizontalArrangement = Arrangement.Center,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (topSite is TopSite.Pinned || topSite is TopSite.Default) {
                    Image(
                        painter = painterResource(id = R.drawable.ic_new_pin),
                        contentDescription = null,
                    )

                    Spacer(modifier = Modifier.width(2.dp))
                }

                Text(
                    text = topSite.title ?: topSite.url,
                    color = FirefoxTheme.colors.textPrimary,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                    style = FirefoxTheme.typography.caption,
                )
            }

            Text(
                text = stringResource(id = R.string.top_sites_sponsored_label),
                modifier = Modifier
                    .width(TOP_SITES_ITEM_SIZE.dp)
                    .alpha(alpha = if (topSite is TopSite.Provided) 1f else 0f),
                color = FirefoxTheme.colors.textSecondary,
                fontSize = 10.sp,
                textAlign = TextAlign.Center,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
        }

        ContextualMenu(
            menuItems = menuItems,
            showMenu = menuExpanded,
            onDismissRequest = { menuExpanded = false },
        )
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
        modifier = Modifier.size(TOP_SITES_FAVICON_CARD_SIZE.dp),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.layer2,
        elevation = 6.dp,
    ) {
        Box(contentAlignment = Alignment.Center) {
            Surface(
                modifier = Modifier.size(TOP_SITES_FAVICON_SIZE.dp),
                color = FirefoxTheme.colors.layer2,
                shape = RoundedCornerShape(4.dp),
            ) {
                val drawableForUrl = getDrawableForUrl(topSite.url)
                when {
                    drawableForUrl != null -> {
                        FaviconImage(painterResource(drawableForUrl))
                    }

                    topSite is TopSite.Provided -> {
                        FaviconBitmap(topSite)
                    }

                    else -> {
                        FaviconDefault(topSite.url)
                    }
                }
            }
        }
    }
}

@Composable
private fun FaviconImage(painter: Painter) {
    Image(
        painter = painter,
        contentDescription = null,
        modifier = Modifier.size(TOP_SITES_FAVICON_SIZE.dp),
        contentScale = ContentScale.Crop,
    )
}

@Composable
private fun FaviconBitmap(topSite: TopSite.Provided) {
    var faviconBitmapUiState by remember { mutableStateOf<FaviconBitmapUiState>(FaviconBitmapUiState.Loading) }

    val client = LocalContext.current.components.core.client

    LaunchedEffect(topSite.imageUrl) {
        val bitmapForUrl = client.bitmapForUrl(topSite.imageUrl)

        faviconBitmapUiState = if (bitmapForUrl == null) {
            FaviconBitmapUiState.Error
        } else {
            FaviconBitmapUiState.Data(bitmapForUrl.asImageBitmap())
        }
    }

    when (val uiState = faviconBitmapUiState) {
        is FaviconBitmapUiState.Loading, FaviconBitmapUiState.Error -> FaviconDefault(topSite.url)
        is FaviconBitmapUiState.Data -> FaviconImage(BitmapPainter(uiState.imageBitmap))
    }
}

private sealed class FaviconBitmapUiState {
    data class Data(val imageBitmap: ImageBitmap) : FaviconBitmapUiState()
    object Loading : FaviconBitmapUiState()
    object Error : FaviconBitmapUiState()
}

@Composable
private fun FaviconDefault(url: String) {
    Favicon(url = url, size = TOP_SITES_FAVICON_SIZE.dp)
}

private fun getDrawableForUrl(url: String) =
    when (url) {
        SupportUtils.POCKET_TRENDING_URL -> R.drawable.ic_pocket
        SupportUtils.BAIDU_URL -> R.drawable.ic_baidu
        SupportUtils.JD_URL -> R.drawable.ic_jd
        SupportUtils.PDD_URL -> R.drawable.ic_pdd
        SupportUtils.TC_URL -> R.drawable.ic_tc
        SupportUtils.MEITUAN_URL -> R.drawable.ic_meituan
        else -> null
    }

@Composable
@Suppress("LongParameterList")
private fun getMenuItems(
    topSite: TopSite,
    onOpenInPrivateTabClicked: (topSite: TopSite) -> Unit,
    onRenameTopSiteClicked: (topSite: TopSite) -> Unit,
    onRemoveTopSiteClicked: (topSite: TopSite) -> Unit,
    onSettingsClicked: () -> Unit,
    onSponsorPrivacyClicked: () -> Unit,
): List<MenuItem> {
    val isPinnedSite = topSite is TopSite.Pinned || topSite is TopSite.Default
    val isProvidedSite = topSite is TopSite.Provided
    val result = mutableListOf<MenuItem>()

    result.add(
        MenuItem(
            title = stringResource(id = R.string.bookmark_menu_open_in_private_tab_button),
            onClick = { onOpenInPrivateTabClicked(topSite) },
        ),
    )

    if (isPinnedSite) {
        result.add(
            MenuItem(
                title = stringResource(id = R.string.rename_top_site),
                onClick = { onRenameTopSiteClicked(topSite) },
            ),
        )
    }

    if (!isProvidedSite) {
        result.add(
            MenuItem(
                title = stringResource(
                    id = if (isPinnedSite) {
                        R.string.remove_top_site
                    } else {
                        R.string.delete_from_history
                    },
                ),
                onClick = { onRemoveTopSiteClicked(topSite) },
            ),
        )
    }

    if (isProvidedSite) {
        result.add(
            MenuItem(
                title = stringResource(id = R.string.delete_from_history),
                onClick = { onRemoveTopSiteClicked(topSite) },
            ),
        )
    }

    if (isProvidedSite) {
        result.add(
            MenuItem(
                title = stringResource(id = R.string.top_sites_menu_settings),
                onClick = onSettingsClicked,
            ),
        )
    }

    if (isProvidedSite) {
        result.add(
            MenuItem(
                title = stringResource(id = R.string.top_sites_menu_sponsor_privacy),
                onClick = onSponsorPrivacyClicked,
            ),
        )
    }

    return result
}

@Composable
@LightDarkPreview
private fun TopSitesPreview() {
    FirefoxTheme {
        Box(modifier = Modifier.background(color = FirefoxTheme.colors.layer1)) {
            TopSites(
                topSites = mutableListOf<TopSite>().apply {
                    for (index in 0 until 2) {
                        add(
                            TopSite.Pinned(
                                id = index.toLong(),
                                title = "Mozilla$index",
                                url = "mozilla.com",
                                createdAt = 0L,
                            ),
                        )
                    }

                    for (index in 0 until 2) {
                        add(
                            TopSite.Provided(
                                id = index.toLong(),
                                title = "Mozilla$index",
                                url = "mozilla.com",
                                clickUrl = "https://mozilla.com/click",
                                imageUrl = "https://test.com/image2.jpg",
                                impressionUrl = "https://example.com",
                                createdAt = 0L,
                            ),
                        )
                    }

                    for (index in 0 until 2) {
                        add(
                            TopSite.Default(
                                id = index.toLong(),
                                title = "Mozilla$index",
                                url = "mozilla.com",
                                createdAt = 0L,
                            ),
                        )
                    }

                    add(
                        TopSite.Default(
                            id = null,
                            title = "Top Articles",
                            url = "https://getpocket.com/fenix-top-articles",
                            createdAt = 0L,
                        ),
                    )
                },
                onTopSiteClick = {},
                onTopSiteLongClick = {},
                onOpenInPrivateTabClicked = {},
                onRenameTopSiteClicked = {},
                onRemoveTopSiteClicked = {},
                onSettingsClicked = {},
                onSponsorPrivacyClicked = {},
            )
        }
    }
}
