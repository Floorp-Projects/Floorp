/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.Snackbar
import androidx.compose.material.SnackbarHost
import androidx.compose.material.SnackbarHostState
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.content.appVersionName
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The home screen of the [DebugDrawer].
 *
 * @param menuItems The list of [DebugDrawerMenuItem]s.
 */
@Composable
fun DebugDrawerHome(
    menuItems: List<DebugDrawerMenuItem>,
) {
    val lazyListState = rememberLazyListState()

    val appName: String
    val appVersion: String
    if (inComposePreview) {
        appName = "App Name Preview"
        appVersion = "100.00.000"
    } else {
        appName = LocalContext.current.appName
        appVersion = LocalContext.current.appVersionName
    }

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .background(color = FirefoxTheme.colors.layer1),
        state = lazyListState,
    ) {
        item(key = "home_header") {
            Row(
                modifier = Modifier
                    .padding(all = 16.dp)
                    .fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(
                    text = appName,
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline5,
                )

                Text(
                    text = appVersion,
                    color = FirefoxTheme.colors.textSecondary,
                    style = FirefoxTheme.typography.headline5,
                )
            }

            Divider()
        }

        items(
            items = menuItems,
            key = { menuItem ->
                menuItem.label
            },
        ) { menuItem ->
            TextListItem(
                label = menuItem.label,
                onClick = menuItem.onClick,
            )

            Divider()
        }
    }
}

@Composable
@LightDarkPreview
private fun DebugDrawerPreview() {
    val scope = rememberCoroutineScope()
    val snackbarState = remember { SnackbarHostState() }

    FirefoxTheme {
        Box {
            DebugDrawerHome(
                menuItems = List(size = 30) {
                    DebugDrawerMenuItem(
                        label = "Navigation $it",
                        onClick = {
                            scope.launch {
                                snackbarState.showSnackbar("$it clicked")
                            }
                        },
                    )
                },
            )

            SnackbarHost(
                hostState = snackbarState,
                modifier = Modifier.align(Alignment.BottomCenter),
            ) { snackbarData ->
                Snackbar(
                    snackbarData = snackbarData,
                )
            }
        }
    }
}
