/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.tabstray.syncedtabs

import android.content.res.Configuration
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.browser.storage.sync.TabEntry
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.PrimaryText
import org.mozilla.fenix.compose.SecondaryText
import org.mozilla.fenix.theme.FirefoxTheme
import mozilla.components.browser.storage.sync.Tab as SyncTab

/**
 * Top-level list UI for displaying Synced Tabs in the Tabs Tray.
 *
 * @param syncedTabs The tab UI items to be displayed.
 * @param onTabClick The lambda for handling clicks on synced tabs.
 */
@Composable
fun SyncedTabsList(syncedTabs: List<SyncedTabsListItem>, onTabClick: (SyncTab) -> Unit) {
    val listState = rememberLazyListState()

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        state = listState,
    ) {
        items(syncedTabs) { syncedTabItem ->
            when (syncedTabItem) {
                is SyncedTabsListItem.Device -> SyncedTabsDeviceItem(deviceName = syncedTabItem.displayName)
                is SyncedTabsListItem.Error -> SyncedTabsErrorItem(
                    errorText = syncedTabItem.errorText,
                    errorButton = syncedTabItem.errorButton
                )
                is SyncedTabsListItem.NoTabs -> SyncedTabsNoTabsItem()
                is SyncedTabsListItem.Tab -> {
                    SyncedTabsTabItem(
                        tabTitleText = syncedTabItem.displayTitle,
                        url = syncedTabItem.displayURL,
                    ) {
                        onTabClick(syncedTabItem.tab)
                    }
                }
            }
        }

        item {
            // The Spacer here is to act as a footer to add padding to the bottom of the list so
            // the FAB or any potential SnackBar doesn't overlap with the items at the end.
            Spacer(Modifier.height(240.dp))
        }
    }
}

/**
 * Text header for sections of synced tabs
 *
 * @param deviceName The name of the user's device connected that has synced tabs.
 */
@Composable
fun SyncedTabsDeviceItem(deviceName: String) {
    Column(Modifier.fillMaxWidth()) {
        PrimaryText(
            text = deviceName,
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 16.dp, top = 16.dp, end = 8.dp, bottom = 8.dp),
            fontSize = 14.sp,
            fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
            maxLines = 1
        )

        Divider(color = FirefoxTheme.colors.borderPrimary)
    }
}

/**
 * Synced tab list item UI
 *
 * @param tabTitleText The tab's display text.
 * @param url The tab's URL.
 * @param onClick The click handler when this synced tab is clicked.
 */
@Composable
fun SyncedTabsTabItem(tabTitleText: String, url: String, onClick: () -> Unit) {
    Column(
        modifier = Modifier
            .clickable(
                onClickLabel = tabTitleText,
                onClick = onClick
            )
            .padding(horizontal = 16.dp)
            .defaultMinSize(minHeight = 56.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        PrimaryText(
            text = tabTitleText,
            modifier = Modifier.fillMaxWidth(),
            fontSize = 16.sp,
            maxLines = 1
        )

        SecondaryText(
            text = url,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 2.dp),
            fontSize = 12.sp,
            maxLines = 1
        )
    }
}

/**
 * Error UI to show if there is one of the errors outlined in [SyncedTabsView.ErrorType].
 *
 * @param errorText The text to be displayed to the user.
 * @param errorButton Optional class to set up and handle any clicks in the Error UI.
 */
@Composable
fun SyncedTabsErrorItem(errorText: String, errorButton: SyncedTabsListItem.ErrorButton? = null) {
    Box(
        Modifier
            .padding(all = 16.dp)
            .height(IntrinsicSize.Min)
    ) {
        val dashColor = FirefoxTheme.colors.borderPrimary

        Canvas(Modifier.fillMaxSize()) {
            drawRoundRect(
                color = dashColor,
                style = Stroke(
                    width = 2.dp.toPx(),
                    pathEffect = PathEffect.dashPathEffect(floatArrayOf(4.dp.toPx(), 4.dp.toPx()), 0f)
                ),
                cornerRadius = CornerRadius(
                    x = 8.dp.toPx(),
                    y = 8.dp.toPx()
                ),
            )
        }

        Column(
            Modifier
                .padding(all = 16.dp)
                .fillMaxWidth()
        ) {
            PrimaryText(
                text = errorText,
                modifier = Modifier.fillMaxWidth(),
                fontSize = 14.sp
            )

            errorButton?.let {
                Spacer(modifier = Modifier.height(12.dp))

                SyncedTabsErrorButton(buttonText = it.buttonText, onClick = it.onClick)
            }
        }
    }
}

/**
 * Error button UI within SyncedTabsErrorItem
 *
 * @param buttonText The error button's text and accessibility hint.
 * @param onClick The lambda called when the button is clicked.
 */
@Composable
fun SyncedTabsErrorButton(buttonText: String, onClick: () -> Unit) {
    Button(
        onClick = onClick,
        modifier = Modifier.clip(RoundedCornerShape(size = 4.dp)),
        elevation = ButtonDefaults.elevation(defaultElevation = 0.dp, pressedElevation = 0.dp),
        colors = ButtonDefaults.outlinedButtonColors(backgroundColor = FirefoxTheme.colors.actionPrimary),
    ) {
        Icon(
            painter = painterResource(R.drawable.ic_sign_in),
            contentDescription = null,
            tint = FirefoxTheme.colors.textOnColorPrimary,
        )

        Spacer(Modifier.width(8.dp))

        Text(
            text = buttonText,
            modifier = Modifier.align(Alignment.CenterVertically),
            color = FirefoxTheme.colors.textOnColorPrimary,
            fontSize = 14.sp,
            fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
            maxLines = 2
        )
    }
}

/**
 * UI to be displayed when a user's device has no synced tabs.
 */
@Composable
fun SyncedTabsNoTabsItem() {
    SecondaryText(
        text = stringResource(R.string.synced_tabs_no_open_tabs),
        modifier = Modifier
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .fillMaxWidth(),
        fontSize = 16.sp,
        maxLines = 1
    )
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun SyncedTabsListItemsPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            SyncedTabsDeviceItem(deviceName = "Google Pixel Pro Max +Ultra 5000")

            Spacer(Modifier.height(16.dp))

            SyncedTabsTabItem(tabTitleText = "Mozilla", url = "www.mozilla.org") { println("Clicked tab") }

            Spacer(Modifier.height(16.dp))

            SyncedTabsErrorItem(errorText = stringResource(R.string.synced_tabs_reauth))

            Spacer(Modifier.height(16.dp))

            SyncedTabsNoTabsItem()

            Spacer(Modifier.height(16.dp))
        }
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun SyncedTabsErrorPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            SyncedTabsErrorItem(
                errorText = stringResource(R.string.synced_tabs_no_tabs),
                errorButton = SyncedTabsListItem.ErrorButton(
                    buttonText = stringResource(R.string.synced_tabs_sign_in_button)
                ) {
                    println("SyncedTabsErrorButton click")
                },
            )
        }
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun SyncedTabsListPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            SyncedTabsList(
                getFakeSyncedTabList(),
            ) {
                println("Tab clicked")
            }
        }
    }
}

/**
 * Converts a list of [SyncedDeviceTabs] into a list of [SyncedTabsListItem].
 */
fun List<SyncedDeviceTabs>.toComposeList() = asSequence().flatMap { (device, tabs) ->
    // Transform to sticky headers data here https://github.com/mozilla-mobile/fenix/issues/19942
    val deviceTabs = if (tabs.isEmpty()) {
        sequenceOf(SyncedTabsListItem.NoTabs)
    } else {
        tabs.asSequence().map {
            val url = it.active().url
            val titleText = it.active().title.ifEmpty { url.take(MAX_URI_LENGTH) }
            SyncedTabsListItem.Tab(titleText, url, it)
        }
    }

    sequenceOf(SyncedTabsListItem.Device(device.displayName)) + deviceTabs
}.toList()

/**
 * Helper function to create a List of [SyncedTabsListItem] for previewing.
 */
@VisibleForTesting internal fun getFakeSyncedTabList(): List<SyncedTabsListItem> = listOf(
    SyncedTabsListItem.Device("Device 1"),
    generateFakeTab("Mozilla", "www.mozilla.org"),
    generateFakeTab("Google", "www.google.com"),
    generateFakeTab("", "www.google.com"),
    SyncedTabsListItem.Device("Device 2"),
    SyncedTabsListItem.NoTabs,
    SyncedTabsListItem.Device("Device 3"),
    SyncedTabsListItem.Error("Please re-authenticate"),
)

/**
 * Helper function to create a [SyncedTabsListItem.Tab] for previewing.
 */
private fun generateFakeTab(tabName: String, tabUrl: String): SyncedTabsListItem.Tab = SyncedTabsListItem.Tab(
    tabName.ifEmpty { tabUrl },
    tabUrl,
    SyncTab(
        history = listOf(TabEntry(tabName, tabUrl, null)),
        active = 0,
        lastUsed = 0L,
    )
)
