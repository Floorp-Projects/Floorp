/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.tabstray

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.DismissValue
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.material.rememberDismissState
import androidx.compose.material.ripple.rememberRipple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.thumbnails.storage.ThumbnailStorage
import mozilla.components.support.ktx.kotlin.MAX_URI_LENGTH
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.SwipeToDismiss
import org.mozilla.fenix.compose.TabThumbnail
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.tabstray.TabsTrayTestTag
import org.mozilla.fenix.tabstray.ext.toDisplayTitle
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * List item used to display a tab that supports clicks,
 * long clicks, multiselection, and media controls.
 *
 * @param tab The given tab to be render as view a grid item.
 * @param storage [ThumbnailStorage] to obtain tab thumbnail bitmaps from.
 * @param thumbnailSize Size of tab's thumbnail.
 * @param isSelected Indicates if the item should be render as selected.
 * @param multiSelectionEnabled Indicates if the item should be render with multi selection options,
 * enabled.
 * @param multiSelectionSelected Indicates if the item should be render as multi selection selected
 * option.
 * @param onCloseClick Callback to handle the click event of the close button.
 * @param onMediaClick Callback to handle when the media item is clicked.
 * @param onClick Callback to handle when item is clicked.
 * @param onLongClick Callback to handle when item is long clicked.
 */
@OptIn(ExperimentalFoundationApi::class, ExperimentalMaterialApi::class)
@Composable
@Suppress("MagicNumber", "LongMethod", "LongParameterList")
fun TabListItem(
    tab: TabSessionState,
    storage: ThumbnailStorage,
    thumbnailSize: Int,
    isSelected: Boolean = false,
    multiSelectionEnabled: Boolean = false,
    multiSelectionSelected: Boolean = false,
    onCloseClick: (tab: TabSessionState) -> Unit,
    onMediaClick: (tab: TabSessionState) -> Unit,
    onClick: (tab: TabSessionState) -> Unit,
    onLongClick: (tab: TabSessionState) -> Unit,
) {
    val contentBackgroundColor = if (isSelected) {
        FirefoxTheme.colors.layerAccentNonOpaque
    } else {
        FirefoxTheme.colors.layer1
    }

    val dismissState = rememberDismissState(
        confirmStateChange = { dismissValue ->
            if (dismissValue == DismissValue.DismissedToEnd || dismissValue == DismissValue.DismissedToStart) {
                onCloseClick(tab)
                true
            } else {
                false
            }
        },
    )

    // Used to propagate the ripple effect to the whole tab
    val interactionSource = remember { MutableInteractionSource() }

    SwipeToDismiss(
        state = dismissState,
        enabled = !multiSelectionEnabled,
        backgroundContent = {
            DismissedTabBackground(dismissState.dismissDirection, RoundedCornerShape(0.dp))
        },
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer3)
                .background(contentBackgroundColor)
                .combinedClickable(
                    interactionSource = interactionSource,
                    indication = rememberRipple(
                        color = when (isSystemInDarkTheme()) {
                            true -> PhotonColors.White
                            false -> PhotonColors.Black
                        },
                    ),
                    onLongClick = { onLongClick(tab) },
                    onClick = { onClick(tab) },
                )
                .padding(start = 16.dp, top = 8.dp, bottom = 8.dp)
                .testTag(TabsTrayTestTag.tabItemRoot),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Thumbnail(
                tab = tab,
                size = thumbnailSize,
                storage = storage,
                multiSelectionEnabled = multiSelectionEnabled,
                isSelected = multiSelectionSelected,
                onMediaIconClicked = { onMediaClick(it) },
                interactionSource = interactionSource,
            )

            Column(
                modifier = Modifier
                    .padding(start = 12.dp)
                    .weight(weight = 1f),
            ) {
                Text(
                    text = tab.toDisplayTitle().take(MAX_URI_LENGTH),
                    color = FirefoxTheme.colors.textPrimary,
                    fontSize = 16.sp,
                    letterSpacing = 0.0.sp,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 2,
                )

                Text(
                    text = tab.content.url.toShortUrl(),
                    color = FirefoxTheme.colors.textSecondary,
                    fontSize = 14.sp,
                    letterSpacing = 0.0.sp,
                    overflow = TextOverflow.Ellipsis,
                    maxLines = 1,
                )
            }

            if (!multiSelectionEnabled) {
                IconButton(
                    onClick = { onCloseClick(tab) },
                    modifier = Modifier
                        .size(size = 48.dp)
                        .testTag(TabsTrayTestTag.tabItemClose),
                ) {
                    Icon(
                        painter = painterResource(id = R.drawable.mozac_ic_cross_24),
                        contentDescription = stringResource(
                            id = R.string.close_tab_title,
                            tab.content.title,
                        ),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            } else {
                Spacer(modifier = Modifier.size(48.dp))
            }
        }
    }
}

@Composable
@Suppress("LongParameterList")
private fun Thumbnail(
    tab: TabSessionState,
    size: Int,
    storage: ThumbnailStorage,
    multiSelectionEnabled: Boolean,
    isSelected: Boolean,
    onMediaIconClicked: ((TabSessionState) -> Unit),
    interactionSource: MutableInteractionSource,
) {
    Box {
        TabThumbnail(
            tab = tab,
            size = size,
            storage = storage,
            modifier = Modifier
                .size(width = 92.dp, height = 72.dp)
                .semantics(mergeDescendants = true) {
                    testTag = TabsTrayTestTag.tabItemThumbnail
                },
            contentDescription = stringResource(id = R.string.mozac_browser_tabstray_open_tab),
        )

        if (isSelected) {
            Box(
                modifier = Modifier
                    .size(width = 92.dp, height = 72.dp)
                    .clip(RoundedCornerShape(4.dp))
                    .background(FirefoxTheme.colors.layerAccentNonOpaque),
            )

            Card(
                modifier = Modifier
                    .size(size = 40.dp)
                    .align(alignment = Alignment.Center),
                shape = CircleShape,
                backgroundColor = FirefoxTheme.colors.layerAccent,
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_checkmark_24),
                    modifier = Modifier
                        .matchParentSize()
                        .padding(all = 8.dp),
                    contentDescription = null,
                    tint = colorResource(id = R.color.mozac_ui_icons_fill),
                )
            }
        }

        if (!multiSelectionEnabled) {
            MediaImage(
                tab = tab,
                onMediaIconClicked = onMediaIconClicked,
                modifier = Modifier.align(Alignment.TopEnd),
                interactionSource = interactionSource,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun TabListItemPreview() {
    FirefoxTheme {
        TabListItem(
            tab = createTab(url = "www.mozilla.com", title = "Mozilla"),
            thumbnailSize = 108,
            storage = ThumbnailStorage(LocalContext.current),
            onCloseClick = {},
            onMediaClick = {},
            onClick = {},
            onLongClick = {},
        )
    }
}

@Composable
@LightDarkPreview
private fun SelectedTabListItemPreview() {
    FirefoxTheme {
        TabListItem(
            tab = createTab(url = "www.mozilla.com", title = "Mozilla"),
            thumbnailSize = 108,
            storage = ThumbnailStorage(LocalContext.current),
            onCloseClick = {},
            onMediaClick = {},
            onClick = {},
            onLongClick = {},
            multiSelectionEnabled = true,
            multiSelectionSelected = true,
        )
    }
}
