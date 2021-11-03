/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.icons.compose.Loader
import mozilla.components.browser.icons.compose.Placeholder
import mozilla.components.browser.icons.compose.WithIcon
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.Image
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A list of recent bookmarks.
 *
 * @param bookmarks List of [RecentBookmark]s to display.
 * @param onRecentBookmarkClick Invoked when the user clicks on a recent bookmark.
 */
@Composable
fun RecentBookmarks(
    bookmarks: List<RecentBookmark>,
    onRecentBookmarkClick: (RecentBookmark) -> Unit = {}
) {
    LazyRow(
        contentPadding = PaddingValues(horizontal = 16.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        items(bookmarks) { bookmark ->
            RecentBookmarkItem(
                bookmark = bookmark,
                onRecentBookmarkClick = onRecentBookmarkClick
            )
        }
    }
}

/**
 * A recent bookmark item.
 *
 * @param bookmark The [RecentBookmark] to display.
 * @param onRecentBookmarkClick Invoked when the user clicks on the recent bookmark item.
 */
@Composable
private fun RecentBookmarkItem(
    bookmark: RecentBookmark,
    onRecentBookmarkClick: (RecentBookmark) -> Unit = {}
) {
    Column(
        modifier = Modifier
            .width(156.dp)
            .clickable { onRecentBookmarkClick(bookmark) }
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(96.dp),
            elevation = 6.dp
        ) {
            RecentBookmarkImage(bookmark)
        }

        Spacer(modifier = Modifier.height(8.dp))

        Text(
            text = bookmark.title ?: bookmark.url ?: "",
            color = FirefoxTheme.colors.textPrimary,
            fontSize = 12.sp,
            overflow = TextOverflow.Ellipsis,
            maxLines = 1
        )
    }
}

@Composable
private fun RecentBookmarkImage(bookmark: RecentBookmark) {
    when {
        !bookmark.previewImageUrl.isNullOrEmpty() -> {
            Image(
                url = bookmark.previewImageUrl,
                modifier = Modifier
                    .size(156.dp, 96.dp),
                targetSize = 156.dp,
                contentScale = ContentScale.Crop
            )
        }
        !bookmark.url.isNullOrEmpty() -> {
            components.core.icons.Loader(bookmark.url) {
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
        }
    }
}

@Composable
@Preview
private fun RecentBookmarksPreview() {
    FirefoxTheme {
        RecentBookmarks(
            bookmarks = listOf(
                RecentBookmark(
                    title = "Other Bookmark Title",
                    url = "https://www.example.com",
                    previewImageUrl = null
                ),
                RecentBookmark(
                    title = "Other Bookmark Title",
                    url = "https://www.example.com",
                    previewImageUrl = null
                ),
                RecentBookmark(
                    title = "Other Bookmark Title",
                    url = "https://www.example.com",
                    previewImageUrl = null
                ),
                RecentBookmark(
                    title = "Other Bookmark Title",
                    url = "https://www.example.com",
                    previewImageUrl = null
                )
            )
        )
    }
}
