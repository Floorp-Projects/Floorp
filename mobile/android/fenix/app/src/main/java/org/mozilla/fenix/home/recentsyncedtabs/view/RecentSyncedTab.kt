/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.concept.sync.DeviceType
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ThumbnailCard
import org.mozilla.fenix.compose.button.Button
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * A recent synced tab card.
 *
 * @param tab The [RecentSyncedTab] to display.
 * @param onRecentSyncedTabClick Invoked when the user clicks on the recent synced tab.
 * @param onSeeAllSyncedTabsButtonClick Invoked when user clicks on the "See all" button in the synced tab card.
 */
@Suppress("LongMethod")
@Composable
fun RecentSyncedTab(
    tab: RecentSyncedTab?,
    onRecentSyncedTabClick: (RecentSyncedTab) -> Unit,
    onSeeAllSyncedTabsButtonClick: () -> Unit,
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .height(180.dp)
            .clickable { tab?.let { onRecentSyncedTabClick(tab) } },
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.layer2,
        elevation = 6.dp
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(modifier = Modifier.height(IntrinsicSize.Min)) {
                if (tab == null) {
                    RecentTabImagePlaceholder()
                } else {
                    ThumbnailCard(
                        url = tab.url,
                        key = tab.url.hashCode().toString(),
                        modifier = Modifier
                            .size(108.dp, 80.dp)
                            .clip(RoundedCornerShape(8.dp))
                    )
                }

                Spacer(modifier = Modifier.width(16.dp))

                Column(
                    verticalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier.fillMaxHeight()
                ) {
                    if (tab == null) {
                        RecentTabTitlePlaceholder()
                    } else {
                        Text(
                            text = tab.title,
                            color = FirefoxTheme.colors.textPrimary,
                            fontSize = 14.sp,
                            overflow = TextOverflow.Ellipsis,
                            maxLines = 2,
                        )
                    }

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        if (tab == null) {
                            Box(
                                modifier = Modifier
                                    .background(FirefoxTheme.colors.layer3)
                                    .size(18.dp)
                            )
                        } else {
                            Image(
                                painter = painterResource(R.drawable.ic_synced_tabs),
                                contentDescription = stringResource(
                                    R.string.recent_tabs_synced_device_icon_content_description
                                ),
                                modifier = Modifier.size(18.dp)
                            )
                        }

                        Spacer(modifier = Modifier.width(8.dp))

                        if (tab == null) {
                            TextLinePlaceHolder()
                        } else {
                            Text(
                                text = tab.deviceDisplayName,
                                color = FirefoxTheme.colors.textSecondary,
                                fontSize = 12.sp,
                                overflow = TextOverflow.Ellipsis,
                                maxLines = 1,
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(32.dp))

            Button(
                text = if (tab != null) {
                    stringResource(R.string.recent_tabs_see_all_synced_tabs_button_text)
                } else {
                    ""
                },
                textColor = FirefoxTheme.colors.textActionSecondary,
                backgroundColor = if (tab == null) {
                    FirefoxTheme.colors.layer3
                } else {
                    FirefoxTheme.colors.actionSecondary
                },
                tint = FirefoxTheme.colors.iconActionSecondary,
                onClick = onSeeAllSyncedTabsButtonClick,
            )
        }
    }
}

/**
 * A placeholder for a recent tab image.
 */
@Composable
private fun RecentTabImagePlaceholder() {
    Box(
        modifier = Modifier
            .size(108.dp, 80.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(color = FirefoxTheme.colors.layer3)
    )
}

/**
 * A placeholder for a tab title.
 */
@Composable
private fun RecentTabTitlePlaceholder() {
    Column {
        TextLinePlaceHolder()

        Spacer(modifier = Modifier.height(8.dp))

        TextLinePlaceHolder()
    }
}

/**
 * A placeholder for a single line of text.
 */
@Composable
private fun TextLinePlaceHolder() {
    Box(
        modifier = Modifier
            .height(12.dp)
            .fillMaxWidth()
            .background(FirefoxTheme.colors.layer3)
    )
}

@Preview
@Composable
private fun LoadedRecentSyncedTab() {
    val tab = RecentSyncedTab(
        deviceDisplayName = "Firefox on MacBook",
        deviceType = DeviceType.DESKTOP,
        title = "This is a long site title",
        url = "https://mozilla.org",
        iconUrl = "https://mozilla.org",
    )
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        RecentSyncedTab(
            tab = tab,
            onRecentSyncedTabClick = {},
            onSeeAllSyncedTabsButtonClick = {},
        )
    }
}

@Preview
@Composable
private fun LoadingRecentSyncedTab() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        RecentSyncedTab(
            tab = null,
            onRecentSyncedTabClick = {},
            onSeeAllSyncedTabsButtonClick = {},
        )
    }
}
