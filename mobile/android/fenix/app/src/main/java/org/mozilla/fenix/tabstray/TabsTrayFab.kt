/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Floating action button for tabs tray.
 *
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState].
 * @param isSignedIn Used to know when to show the SYNC FAB when [Page.SyncedTabs] is displayed.
 * @param onNormalTabsFabClicked Invoked when the fab is clicked in [Page.NormalTabs].
 * @param onPrivateTabsFabClicked Invoked when the fab is clicked in [Page.PrivateTabs].
 * @param onSyncedTabsFabClicked Invoked when the fab is clicked in [Page.SyncedTabs].
 */
@Composable
fun TabsTrayFab(
    tabsTrayStore: TabsTrayStore,
    isSignedIn: Boolean,
    onNormalTabsFabClicked: () -> Unit,
    onPrivateTabsFabClicked: () -> Unit,
    onSyncedTabsFabClicked: () -> Unit,
) {
    val currentPage by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.selectedPage,
    ) { state -> state.selectedPage }
    val isSyncing by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.syncing,
    ) { state -> state.syncing }
    val isInNormalMode by tabsTrayStore.observeAsState(
        initialValue = tabsTrayStore.state.mode == TabsTrayState.Mode.Normal,
    ) { state -> state.mode == TabsTrayState.Mode.Normal }

    val icon: Painter
    val contentDescription: String
    val label: String?
    val onClick: () -> Unit

    when (currentPage) {
        Page.NormalTabs -> {
            icon = painterResource(id = R.drawable.ic_new)
            contentDescription = stringResource(id = R.string.add_tab)
            label = null
            onClick = onNormalTabsFabClicked
        }

        Page.SyncedTabs -> {
            icon = painterResource(id = R.drawable.ic_fab_sync)
            contentDescription = stringResource(id = R.string.resync_button_content_description)
            label = if (isSyncing) {
                stringResource(id = R.string.sync_syncing_in_progress)
            } else {
                stringResource(id = R.string.tab_drawer_fab_sync)
            }.uppercase()
            onClick = onSyncedTabsFabClicked
        }

        Page.PrivateTabs -> {
            icon = painterResource(id = R.drawable.ic_new)
            contentDescription = stringResource(id = R.string.add_private_tab)
            label = stringResource(id = R.string.tab_drawer_fab_content).uppercase()
            onClick = onPrivateTabsFabClicked
        }
    }

    if (isInNormalMode && !(currentPage == Page.SyncedTabs && !isSignedIn)) {
        FloatingActionButton(
            icon = icon,
            modifier = Modifier
                .padding(bottom = 16.dp, end = 16.dp)
                .testTag(TabsTrayTestTag.fab),
            contentDescription = contentDescription,
            label = label,
            onClick = onClick,
        )
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
        TabsTrayFab(
            tabsTrayStore = store,
            isSignedIn = true,
            onNormalTabsFabClicked = {},
            onPrivateTabsFabClicked = {},
            onSyncedTabsFabClicked = {},
        )
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
        TabsTrayFab(
            tabsTrayStore = store,
            isSignedIn = true,
            onNormalTabsFabClicked = {},
            onPrivateTabsFabClicked = {},
            onSyncedTabsFabClicked = {},
        )
    }
}
