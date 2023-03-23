/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Floating action button for tabs tray.
 *
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 */
@Composable
fun TabsTrayFab(
    tabsTrayStore: TabsTrayStore,
) {
    val currentPage: Page = tabsTrayStore.observeAsComposableState { state ->
        state.selectedPage
    }.value ?: Page.NormalTabs
    val isSyncing: Boolean = tabsTrayStore.observeAsComposableState { state ->
        state.syncing
    }.value ?: false
    val isInNormalMode: Boolean = tabsTrayStore.observeAsComposableState { state ->
        state.mode == TabsTrayState.Mode.Normal
    }.value ?: false

    val icon: Painter
    val contentDescription: String
    val label: String?

    when (currentPage) {
        Page.NormalTabs -> {
            icon = painterResource(id = R.drawable.ic_new)
            contentDescription = stringResource(id = R.string.add_tab)
            label = null
        }

        Page.SyncedTabs -> {
            icon = painterResource(id = R.drawable.ic_fab_sync)
            contentDescription = stringResource(id = R.string.tab_drawer_fab_sync)
            label = if (isSyncing) {
                stringResource(id = R.string.sync_syncing_in_progress)
            } else {
                stringResource(id = R.string.resync_button_content_description)
            }.uppercase()
        }

        Page.PrivateTabs -> {
            icon = painterResource(id = R.drawable.ic_new)
            contentDescription = stringResource(id = R.string.add_private_tab)
            label = stringResource(id = R.string.tab_drawer_fab_content).uppercase()
        }
    }

    Box(Modifier.fillMaxSize()) {
        if (isInNormalMode) {
            FloatingActionButton(
                icon = icon,
                modifier = Modifier
                    .align(Alignment.BottomEnd)
                    .padding(16.dp),
                contentDescription = contentDescription,
                label = label,
            ) {}
        }
    }
}

@LightDarkPreview
@Composable
private fun TabsTraySyncFabPreview() {
    val store = TabsTrayStore(
        initialState = TabsTrayState(
            selectedPage = Page.SyncedTabs,
            syncing = true,
        ),
    )

    FirefoxTheme {
        TabsTrayFab(store)
    }
}

@LightDarkPreview
@Composable
private fun TabsTrayPrivateFabPreview() {
    val store = TabsTrayStore(
        initialState = TabsTrayState(
            selectedPage = Page.PrivateTabs,
        ),
    )
    FirefoxTheme {
        TabsTrayFab(store)
    }
}
