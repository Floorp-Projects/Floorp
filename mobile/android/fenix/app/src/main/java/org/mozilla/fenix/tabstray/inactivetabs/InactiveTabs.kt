/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.tabstray.inactivetabs

import android.content.res.Configuration
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.compose.list.ExpandableListHeader
import org.mozilla.fenix.compose.list.FaviconListItem
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.tabstray.ext.toDisplayTitle
import org.mozilla.fenix.theme.FirefoxTheme

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(8.dp)

/**
 * Top-level list for displaying an expandable section of Inactive Tabs.
 *
 * @param inactiveTabs List of [TabSessionState] to display.
 * @param expanded Whether to show the inactive tabs section expanded or collapsed.
 * @param showAutoCloseDialog Whether to show the auto close inactive tabs dialog.
 * @param onHeaderClick Called when the user clicks on the inactive tabs section header.
 * @param onDeleteAllButtonClick Called when the user clicks on the delete all inactive tabs button.
 * @param onAutoCloseDismissClick Called when the user clicks on the auto close dialog's dismiss button.
 * @param onEnableAutoCloseClick Called when the user clicks on the auto close dialog's enable button.
 * @param onTabClick Called when the user clicks on a specific inactive tab.
 * @param onTabCloseClick Called when the user clicks on a specific inactive tab's close button.
 */
@Composable
@Suppress("LongParameterList")
fun InactiveTabsList(
    inactiveTabs: List<TabSessionState>,
    expanded: Boolean,
    showAutoCloseDialog: Boolean,
    onHeaderClick: (Boolean) -> Unit,
    onDeleteAllButtonClick: () -> Unit,
    onAutoCloseDismissClick: () -> Unit,
    onEnableAutoCloseClick: () -> Unit,
    onTabClick: (TabSessionState) -> Unit,
    onTabCloseClick: (TabSessionState) -> Unit,
) {
    Card(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
        shape = ROUNDED_CORNER_SHAPE,
        backgroundColor = FirefoxTheme.colors.layer2,
        border = BorderStroke(
            width = 1.dp,
            color = FirefoxTheme.colors.borderPrimary,
        ),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
        ) {
            InactiveTabsHeader(
                expanded = expanded,
                onClick = { onHeaderClick(!expanded) },
                onDeleteAllClick = onDeleteAllButtonClick,
            )

            if (expanded) {
                if (showAutoCloseDialog) {
                    InactiveTabsAutoClosePrompt(
                        onDismissClick = onAutoCloseDismissClick,
                        onEnableAutoCloseClick = onEnableAutoCloseClick,
                    )
                }

                Column {
                    inactiveTabs.forEach { tab ->
                        val tabUrl = tab.content.url.toShortUrl()
                        val faviconPainter = tab.content.icon?.run {
                            prepareToDraw()
                            BitmapPainter(asImageBitmap())
                        }

                        FaviconListItem(
                            label = tab.toDisplayTitle(),
                            description = tabUrl,
                            faviconPainter = faviconPainter,
                            onClick = { onTabClick(tab) },
                            url = tabUrl,
                            iconPainter = painterResource(R.drawable.mozac_ic_cross_24),
                            iconDescription = stringResource(R.string.content_description_close_button),
                            onIconClick = { onTabCloseClick(tab) },
                        )
                    }
                }

                Spacer(modifier = Modifier.height(8.dp))
            }
        }
    }
}

/**
 * Collapsible header for the Inactive Tabs section.
 *
 * @param expanded Whether the section is expanded.
 * @param onClick Called when the user clicks on the header.
 * @param onDeleteAllClick Called when the user clicks on the delete all button.
 */
@Composable
private fun InactiveTabsHeader(
    expanded: Boolean,
    onClick: () -> Unit,
    onDeleteAllClick: () -> Unit,
) {
    ExpandableListHeader(
        headerText = stringResource(R.string.inactive_tabs_title),
        headerTextStyle = FirefoxTheme.typography.headline7,
        expanded = expanded,
        expandActionContentDescription = stringResource(R.string.inactive_tabs_expand_content_description),
        collapseActionContentDescription = stringResource(R.string.inactive_tabs_collapse_content_description),
        onClick = onClick,
    ) {
        IconButton(
            onClick = onDeleteAllClick,
            modifier = Modifier.padding(horizontal = 4.dp),
        ) {
            Icon(
                painter = painterResource(R.drawable.ic_delete),
                contentDescription = stringResource(R.string.inactive_tabs_delete_all),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        }
    }
}

/**
 * Inactive Tabs auto close dialog.
 *
 * @param onDismissClick Called when the user clicks on the auto close dialog's dismiss button.
 * @param onEnableAutoCloseClick Called when the user clicks on the auto close dialog's enable button.
 */
@Composable
private fun InactiveTabsAutoClosePrompt(
    onDismissClick: () -> Unit,
    onEnableAutoCloseClick: () -> Unit,
) {
    Card(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
        shape = ROUNDED_CORNER_SHAPE,
        backgroundColor = FirefoxTheme.colors.layer2,
        border = BorderStroke(
            width = 1.dp,
            color = FirefoxTheme.colors.borderPrimary,
        ),
    ) {
        Column(
            modifier = Modifier.padding(horizontal = 12.dp),
            horizontalAlignment = Alignment.End,
        ) {
            Spacer(modifier = Modifier.height(12.dp))

            Row(
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = stringResource(R.string.tab_tray_inactive_auto_close_title),
                    color = FirefoxTheme.colors.textPrimary,
                    modifier = Modifier.weight(1f),
                    style = FirefoxTheme.typography.headline8,
                )

                IconButton(
                    onClick = onDismissClick,
                    modifier = Modifier.size(20.dp),
                ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_close_20),
                        contentDescription =
                        stringResource(R.string.tab_tray_inactive_auto_close_button_content_description),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            }

            Text(
                text = stringResource(
                    R.string.tab_tray_inactive_auto_close_body_2,
                    stringResource(R.string.app_name),
                ),
                color = FirefoxTheme.colors.textSecondary,
                modifier = Modifier.fillMaxWidth(),
                fontSize = 14.sp,
            )

            TextButton(
                text = stringResource(R.string.tab_tray_inactive_turn_on_auto_close_button_2),
                onClick = onEnableAutoCloseClick,
            )
        }
    }
}

@Composable
@Preview(name = "Auto close dialog dark", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(name = "Auto close dialog light", uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun InactiveTabsAutoClosePromptPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            InactiveTabsAutoClosePrompt(
                onDismissClick = {},
                onEnableAutoCloseClick = {},
            )
        }
    }
}

@Composable
@Preview(name = "Full preview dark", uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(name = "Full preview light", uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun InactiveTabsListPreview() {
    var expanded by remember { mutableStateOf(true) }
    var showAutoClosePrompt by remember { mutableStateOf(true) }

    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            InactiveTabsList(
                inactiveTabs = generateFakeInactiveTabsList(),
                expanded = expanded,
                showAutoCloseDialog = showAutoClosePrompt,
                onHeaderClick = { expanded = !expanded },
                onDeleteAllButtonClick = {},
                onAutoCloseDismissClick = { showAutoClosePrompt = !showAutoClosePrompt },
                onEnableAutoCloseClick = { showAutoClosePrompt = !showAutoClosePrompt },
                onTabClick = {},
                onTabCloseClick = {},
            )
        }
    }
}

private fun generateFakeInactiveTabsList(): List<TabSessionState> =
    listOf(
        TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            ),
        ),
        TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.google.com",
            ),
        ),
    )
