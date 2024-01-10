/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.navigation.compose.rememberNavController
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.debugsettings.store.DebugDrawerAction
import org.mozilla.fenix.debugsettings.store.DebugDrawerStore
import org.mozilla.fenix.debugsettings.store.DrawerStatus
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Overlay for presenting Fenix-wide debugging content.
 */
@Composable
fun FenixOverlay() {
    val navController = rememberNavController()
    val debugDrawerStore = remember { DebugDrawerStore() }
    val drawerStatus by debugDrawerStore.observeAsState(initialValue = DrawerStatus.Closed) { state ->
        state.drawerStatus
    }

    FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
        DebugOverlay(
            navController = navController,
            drawerStatus = drawerStatus,
            onDrawerOpen = {
                debugDrawerStore.dispatch(DebugDrawerAction.DrawerOpened)
            },
            onDrawerClose = {
                debugDrawerStore.dispatch(DebugDrawerAction.DrawerClosed)
            },
            onBackButtonClick = {},
        )
    }
}

@LightDarkPreview
@Composable
private fun FenixOverlayPreview() {
    FenixOverlay()
}
