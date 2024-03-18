/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.topsites

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.focus.Components
import org.mozilla.focus.GleanMetrics.Shortcuts
import org.mozilla.focus.components
import org.mozilla.focus.state.AppAction

@OptIn(DelicateCoroutinesApi::class)
@Composable
fun TopSitesOverlay(modifier: Modifier = Modifier) {
    val components = components
    val topSitesState = components.appStore.observeAsComposableState { state -> state.topSites }
    val topSites = topSitesState.value ?: listOf()
    val showRenameDialog: MutableState<Boolean> = remember { mutableStateOf(false) }
    val topSiteItem: MutableState<TopSite?> = remember { mutableStateOf(null) }

    if (!topSites.isNullOrEmpty()) {
        Column(
            modifier = modifier,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Spacer(modifier = Modifier.height(24.dp))

            TopSites(
                topSites = topSites,
                onTopSiteClicked = { item -> openTopSite(item, components) },
                onRemoveTopSiteClicked = { item ->
                    removeTopSite(item, components)
                },
                onRenameTopSiteClicked = { topSite ->
                    showRenameDialog.value = true
                    topSiteItem.value = topSite
                },
            )
            Spacer(modifier = Modifier.height(24.dp))
        }

        if (showRenameDialog.value) {
            topSiteItem.value?.let { selectedTopSite ->
                RenameTopSiteDialog(
                    currentName = selectedTopSite.title ?: "",
                    onConfirm = { newTitle ->
                        renameTopSite(selectedTopSite, newTitle, components)
                        showRenameDialog.value = false
                        topSiteItem.value = null
                    },
                    onDismiss = {
                        showRenameDialog.value = false
                        topSiteItem.value = null
                    },
                )
            }
        }
    }
}

private fun openTopSite(item: TopSite, components: Components) {
    val currentTabId = components.store.state.selectedTabId
    if (currentTabId.isNullOrEmpty()) {
        components.tabsUseCases.addTab(
            url = item.url,
            source = SessionState.Source.Internal.HomeScreen,
            selectTab = true,
            private = true,
        )
    } else {
        components.sessionUseCases.loadUrl(item.url, currentTabId)
        components.appStore.dispatch(AppAction.FinishEdit(currentTabId))
    }

    Shortcuts.shortcutOpenedCounter.add()
}

@OptIn(DelicateCoroutinesApi::class)
fun removeTopSite(item: TopSite, components: Components) {
    Shortcuts.shortcutRemovedCounter["removed_from_home_screen"].add()

    GlobalScope.launch(Dispatchers.IO) {
        components.topSitesUseCases.removeTopSites(item)
    }
}

@OptIn(DelicateCoroutinesApi::class)
fun renameTopSite(selectedTopSite: TopSite, newTitle: String, components: Components) {
    GlobalScope.launch(Dispatchers.IO) {
        components.topSitesUseCases.updateTopSites.invoke(
            selectedTopSite,
            newTitle,
            selectedTopSite.url,
        )
    }
}
