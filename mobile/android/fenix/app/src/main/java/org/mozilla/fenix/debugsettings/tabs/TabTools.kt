/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.tabs

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.tabstray.ext.isNormalTabInactive
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.Settings

/**
 * Tab Tools UI for [DebugDrawer] that displays the tab counts and allows easy bulk-opening of tabs.
 *
 * @param store [BrowserStore] used to obtain the tab counts and fire any tab creation actions.
 * @param settings Used to obtain whether the inactive tabs feature is enabled.
 */
@Composable
fun TabTools(
    store: BrowserStore,
    settings: Settings,
) {
    val inactiveTabsEnabled = settings.inactiveTabsAreEnabled
    val tabs by store.observeAsState(initialValue = emptyList()) { state -> state.tabs }
    val totalTabCount = remember(tabs) { tabs.size }
    val privateTabCount = remember(tabs) { tabs.filter { it.content.private }.size }
    val inactiveTabCount = remember(tabs) {
        if (inactiveTabsEnabled) {
            tabs.filter { it.isNormalTabInactive(maxActiveTime) }.size
        } else {
            0
        }
    }
    val activeTabCount = remember(tabs) { totalTabCount - privateTabCount - inactiveTabCount }

    TabToolsContent(
        activeTabCount = activeTabCount,
        inactiveTabCount = inactiveTabCount,
        privateTabCount = privateTabCount,
        totalTabCount = totalTabCount,
        inactiveTabsEnabled = inactiveTabsEnabled,
    )
}

@Composable
private fun TabToolsContent(
    activeTabCount: Int,
    inactiveTabCount: Int,
    privateTabCount: Int,
    totalTabCount: Int,
    inactiveTabsEnabled: Boolean,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(all = 16.dp),
    ) {
        TabCounter(
            activeTabCount = activeTabCount,
            inactiveTabCount = inactiveTabCount,
            privateTabCount = privateTabCount,
            totalTabCount = totalTabCount,
            inactiveTabsEnabled = inactiveTabsEnabled,
        )
    }
}

@Composable
private fun TabCounter(
    activeTabCount: Int,
    inactiveTabCount: Int,
    privateTabCount: Int,
    totalTabCount: Int,
    inactiveTabsEnabled: Boolean,
) {
    Column {
        Text(
            text = stringResource(R.string.debug_drawer_tab_tools_tab_count_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline5,
        )

        Spacer(modifier = Modifier.height(16.dp))

        TabCountRow(
            tabType = stringResource(R.string.debug_drawer_tab_tools_tab_count_normal),
            count = activeTabCount.toString(),
        )

        if (inactiveTabsEnabled) {
            TabCountRow(
                tabType = stringResource(R.string.debug_drawer_tab_tools_tab_count_inactive),
                count = inactiveTabCount.toString(),
            )
        }

        TabCountRow(
            tabType = stringResource(R.string.debug_drawer_tab_tools_tab_count_private),
            count = privateTabCount.toString(),
        )

        Spacer(modifier = Modifier.height(8.dp))

        Divider()

        Spacer(modifier = Modifier.height(8.dp))

        TabCountRow(
            tabType = stringResource(R.string.debug_drawer_tab_tools_tab_count_total),
            count = totalTabCount.toString(),
        )
    }
}

@Composable
private fun TabCountRow(
    tabType: String,
    count: String,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 16.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(
            text = tabType,
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.headline6,
        )

        Text(
            text = count,
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.headline6,
        )
    }
}

private data class TabToolsPreviewModel(
    val activeTabCount: Int = 0,
    val inactiveTabCount: Int = 0,
    val privateTabCount: Int = 0,
    val inactiveTabsEnabled: Boolean = true,
) {
    val totalTabCount = activeTabCount + inactiveTabCount + privateTabCount
}

private class TabToolsPreviewParameterProvider : PreviewParameterProvider<TabToolsPreviewModel> {
    override val values: Sequence<TabToolsPreviewModel>
        get() = sequenceOf(
            TabToolsPreviewModel(
                activeTabCount = 5,
                inactiveTabCount = 7,
                privateTabCount = 10,
                inactiveTabsEnabled = true,
            ),
            TabToolsPreviewModel(
                activeTabCount = 25,
                inactiveTabsEnabled = false,
            ),
            TabToolsPreviewModel(
                activeTabCount = 50,
                inactiveTabCount = 700,
                privateTabCount = 10,
                inactiveTabsEnabled = true,
            ),
        )
}

@Composable
@LightDarkPreview
private fun TabToolsPreview(
    @PreviewParameter(TabToolsPreviewParameterProvider::class) model: TabToolsPreviewModel,
) {
    with(model) {
        FirefoxTheme {
            Box(
                modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
            ) {
                TabToolsContent(
                    activeTabCount = activeTabCount,
                    inactiveTabCount = inactiveTabCount,
                    privateTabCount = privateTabCount,
                    totalTabCount = totalTabCount,
                    inactiveTabsEnabled = inactiveTabsEnabled,
                )
            }
        }
    }
}
