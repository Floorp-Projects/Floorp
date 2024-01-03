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
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.Text
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import androidx.core.text.isDigitsOnly
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.debugsettings.ui.DebugDrawer
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.tabstray.ext.isNormalTabInactive
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Tab Tools UI for [DebugDrawer] that displays the tab counts and allows easy bulk-opening of tabs.
 *
 * @param store [BrowserStore] used to obtain the tab counts and fire any tab creation actions.
 * @param inactiveTabsEnabled Whether the inactive tabs feature is enabled.
 */
@Composable
fun TabTools(
    store: BrowserStore,
    inactiveTabsEnabled: Boolean,
) {
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
        onCreateTabsClick = { quantity, isInactive, isPrivate ->
            store.dispatch(
                TabListAction.AddMultipleTabsAction(
                    tabs = generateTabList(
                        quantity = quantity,
                        isInactive = isInactive,
                        isPrivate = isPrivate,
                    ),
                ),
            )
        },
    )
}

private fun generateTabList(
    quantity: Int,
    isInactive: Boolean = false,
    isPrivate: Boolean = false,
) = List(quantity) {
    createTab(
        url = "www.example.com",
        private = isPrivate,
        createdAt = if (isInactive) 0L else System.currentTimeMillis(),
    )
}

@Composable
@Suppress("LongParameterList")
private fun TabToolsContent(
    activeTabCount: Int,
    inactiveTabCount: Int,
    privateTabCount: Int,
    totalTabCount: Int,
    inactiveTabsEnabled: Boolean,
    onCreateTabsClick: ((quantity: Int, isInactive: Boolean, isPrivate: Boolean) -> Unit),
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(all = 16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        TabCounter(
            activeTabCount = activeTabCount,
            inactiveTabCount = inactiveTabCount,
            privateTabCount = privateTabCount,
            totalTabCount = totalTabCount,
            inactiveTabsEnabled = inactiveTabsEnabled,
        )

        TabCreationTool(
            inactiveTabsEnabled = inactiveTabsEnabled,
            onCreateTabsClick = onCreateTabsClick,
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

private const val DEFAULT_TABS_TO_ADD = "1"

@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun TabCreationTool(
    inactiveTabsEnabled: Boolean,
    onCreateTabsClick: ((quantity: Int, isInactive: Boolean, isPrivate: Boolean) -> Unit),
) {
    var tabQuantityToCreate by rememberSaveable { mutableStateOf(DEFAULT_TABS_TO_ADD) }
    var hasError by rememberSaveable { mutableStateOf(false) }
    val keyboardController = LocalSoftwareKeyboardController.current

    Column {
        Text(
            text = stringResource(R.string.debug_drawer_tab_tools_tab_creation_tool_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline5,
        )

        TextField(
            value = tabQuantityToCreate,
            onValueChange = {
                tabQuantityToCreate = it
                hasError = it.isEmpty() || !it.isDigitsOnly() || it.toInt() == 0
            },
            modifier = Modifier.fillMaxWidth(),
            textStyle = FirefoxTheme.typography.subtitle1,
            label = {
                Text(
                    text = stringResource(R.string.debug_drawer_tab_tools_tab_creation_tool_text_field_label),
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.caption,
                )
            },
            isError = hasError,
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Number,
            ),
            keyboardActions = KeyboardActions(
                onDone = {
                    keyboardController?.hide()
                },
            ),
            singleLine = true,
            colors = TextFieldDefaults.textFieldColors(
                textColor = FirefoxTheme.colors.textPrimary,
                backgroundColor = Color.Transparent,
                cursorColor = FirefoxTheme.colors.borderFormDefault,
                errorCursorColor = FirefoxTheme.colors.borderWarning,
                focusedIndicatorColor = FirefoxTheme.colors.borderPrimary,
                unfocusedIndicatorColor = FirefoxTheme.colors.borderPrimary,
                errorIndicatorColor = FirefoxTheme.colors.borderWarning,
            ),
        )

        Spacer(modifier = Modifier.height(8.dp))

        PrimaryButton(
            text = stringResource(id = R.string.debug_drawer_tab_tools_tab_creation_tool_button_text_active),
            enabled = !hasError,
            onClick = {
                onCreateTabsClick(tabQuantityToCreate.toInt(), false, false)
            },
        )

        Spacer(modifier = Modifier.height(8.dp))

        if (inactiveTabsEnabled) {
            PrimaryButton(
                text = stringResource(id = R.string.debug_drawer_tab_tools_tab_creation_tool_button_text_inactive),
                enabled = !hasError,
                onClick = {
                    onCreateTabsClick(tabQuantityToCreate.toInt(), true, false)
                },
            )

            Spacer(modifier = Modifier.height(8.dp))
        }

        PrimaryButton(
            text = stringResource(id = R.string.debug_drawer_tab_tools_tab_creation_tool_button_text_private),
            enabled = !hasError,
            onClick = {
                onCreateTabsClick(tabQuantityToCreate.toInt(), false, true)
            },
        )
    }
}

private data class TabToolsPreviewModel(
    val inactiveTabsEnabled: Boolean = true,
)

private class TabToolsPreviewParameterProvider : PreviewParameterProvider<TabToolsPreviewModel> {
    override val values: Sequence<TabToolsPreviewModel>
        get() = sequenceOf(
            TabToolsPreviewModel(
                inactiveTabsEnabled = true,
            ),
            TabToolsPreviewModel(
                inactiveTabsEnabled = false,
            ),
        )
}

@Composable
@LightDarkPreview
private fun TabToolsPreview(
    @PreviewParameter(TabToolsPreviewParameterProvider::class) model: TabToolsPreviewModel,
) {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            TabTools(
                store = BrowserStore(),
                inactiveTabsEnabled = model.inactiveTabsEnabled,
            )
        }
    }
}
