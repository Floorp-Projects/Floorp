/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.tabstray

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.requiredHeight
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.text.BidiFormatter
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.ktx.kotlin.MAX_URI_LENGTH
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.compose.HorizontalFadingEdgeBox
import org.mozilla.fenix.compose.ThumbnailCard
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.tabstray.ext.toDisplayTitle
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Tab grid item used to display a tab that supports clicks,
 * long clicks, multiple selection, and media controls.
 *
 * @param tab The given tab to be render as view a grid item.
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
@OptIn(ExperimentalFoundationApi::class)
@Composable
@Suppress("MagicNumber", "LongParameterList", "LongMethod")
fun TabGridItem(
    tab: TabSessionState,
    isSelected: Boolean = false,
    multiSelectionEnabled: Boolean = false,
    multiSelectionSelected: Boolean = false,
    onCloseClick: (tab: TabSessionState) -> Unit,
    onMediaClick: (tab: TabSessionState) -> Unit,
    onClick: (tab: TabSessionState) -> Unit,
    onLongClick: (tab: TabSessionState) -> Unit,
) {
    val tabBorderModifier = if (isSelected && !multiSelectionEnabled) {
        Modifier.border(
            4.dp,
            FirefoxTheme.colors.borderAccent,
            RoundedCornerShape(12.dp),
        )
    } else {
        Modifier
    }

    Box(
        modifier = Modifier
            .wrapContentHeight()
            .wrapContentWidth(),
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .height(202.dp)
                .padding(4.dp)
                .then(tabBorderModifier)
                .padding(4.dp)
                .combinedClickable(
                    onLongClick = { onLongClick(tab) },
                    onClick = { onClick(tab) },
                ),
            elevation = 0.dp,
            shape = RoundedCornerShape(dimensionResource(id = R.dimen.tab_tray_grid_item_border_radius)),
            border = BorderStroke(1.dp, FirefoxTheme.colors.borderPrimary),
        ) {
            Column(
                modifier = Modifier.background(FirefoxTheme.colors.layer2),
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .wrapContentHeight(),
                ) {
                    Favicon(
                        url = tab.content.url,
                        size = 16.dp,
                        modifier = Modifier
                            .align(Alignment.CenterVertically)
                            .padding(start = 8.dp),
                    )

                    HorizontalFadingEdgeBox(
                        modifier = Modifier
                            .weight(1f)
                            .wrapContentHeight()
                            .requiredHeight(30.dp)
                            .padding(7.dp, 5.dp)
                            .clipToBounds(),
                        backgroundColor = FirefoxTheme.colors.layer2,
                        isContentRtl = BidiFormatter.getInstance().isRtl(tab.content.title),
                    ) {
                        Text(
                            text = tab.toDisplayTitle().take(MAX_URI_LENGTH),
                            fontSize = 14.sp,
                            maxLines = 1,
                            softWrap = false,
                            style = TextStyle(
                                color = FirefoxTheme.colors.textPrimary,
                                textDirection = TextDirection.Content,
                            ),
                        )
                    }

                    Icon(
                        painter = painterResource(id = R.drawable.mozac_ic_close),
                        contentDescription = stringResource(id = R.string.close_tab),
                        tint = FirefoxTheme.colors.iconPrimary,
                        modifier = Modifier
                            .clickable { onCloseClick(tab) }
                            .size(24.dp)
                            .align(Alignment.CenterVertically),

                    )
                }

                Divider()

                Thumbnail(
                    tab = tab,
                    multiSelectionSelected = multiSelectionSelected,
                )
            }
        }

        if (!multiSelectionEnabled) {
            MediaImage(
                tab = tab,
                onMediaIconClicked = { onMediaClick(tab) },
                modifier = Modifier
                    .align(Alignment.TopStart),
            )
        }
    }
}

/**
 * Thumbnail specific for the [TabGridItem], which can be selected.
 *
 * @param tab Tab, containing the thumbnail to be displayed.
 * @param multiSelectionSelected Whether or not the multiple selection is enabled.
 */
@Composable
private fun Thumbnail(
    tab: TabSessionState,
    multiSelectionSelected: Boolean,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer2),
    ) {
        ThumbnailCard(
            url = tab.content.url,
            key = tab.id,
            size = LocalConfiguration.current.screenWidthDp.dp,
            backgroundColor = FirefoxTheme.colors.layer2,
            modifier = Modifier.fillMaxSize(),
        )

        if (multiSelectionSelected) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
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
                    painter = painterResource(id = R.drawable.mozac_ic_check),
                    modifier = Modifier
                        .matchParentSize()
                        .padding(all = 8.dp),
                    contentDescription = null,
                    tint = colorResource(id = R.color.mozac_ui_icons_fill),
                )
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun TabGridItemPreview() {
    FirefoxTheme {
        TabGridItem(
            tab = createTab(
                url = "www.mozilla.com",
                title = "Mozilla Domain",
            ),
            onCloseClick = {},
            onMediaClick = {},
            onClick = {},
            onLongClick = {},
        )
    }
}

@Composable
@LightDarkPreview
private fun TabGridItemSelectedPreview() {
    FirefoxTheme {
        TabGridItem(
            tab = createTab(url = "www.mozilla.com", title = "Mozilla"),
            isSelected = true,
            onCloseClick = {},
            onMediaClick = {},
            onClick = {},
            onLongClick = {},
        )
    }
}

@Composable
@LightDarkPreview
private fun TabGridItemMultiSelectedPreview() {
    FirefoxTheme {
        TabGridItem(
            tab = createTab(url = "www.mozilla.com", title = "Mozilla"),
            multiSelectionEnabled = true,
            multiSelectionSelected = true,
            onCloseClick = {},
            onMediaClick = {},
            onClick = {},
            onLongClick = {},
        )
    }
}
