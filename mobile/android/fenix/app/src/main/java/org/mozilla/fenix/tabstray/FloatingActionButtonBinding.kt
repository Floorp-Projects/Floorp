/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.appcompat.content.res.AppCompatResources
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsInteractor

class FloatingActionButtonBinding(
    private val store: TabsTrayStore,
    private val actionButton: ExtendedFloatingActionButton,
    private val browserTrayInteractor: BrowserTrayInteractor,
    private val syncedTabsInteractor: SyncedTabsInteractor
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun start() {
        setFab(store.state.selectedPage, store.state.syncing)
        scope = store.flowScoped { flow ->
            flow.map { it }
                .ifAnyChanged { state ->
                    arrayOf(
                        state.selectedPage,
                        state.syncing
                    )
                }
                .collect { state ->
                    setFab(state.selectedPage, state.syncing)
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    private fun setFab(selectedPage: Page, syncing: Boolean) {
        when (selectedPage) {
            Page.NormalTabs -> {
                actionButton.apply {
                    shrink()
                    show()
                    icon = AppCompatResources.getDrawable(context, R.drawable.ic_new)
                    setOnClickListener {
                        browserTrayInteractor.onFabClicked(false)
                    }
                }
            }
            Page.PrivateTabs -> {
                actionButton.apply {
                    text = context.getText(R.string.tab_drawer_fab_content)
                    extend()
                    show()
                    icon = AppCompatResources.getDrawable(context, R.drawable.ic_new)
                    setOnClickListener {
                        browserTrayInteractor.onFabClicked(true)
                    }
                }
            }
            Page.SyncedTabs -> {
                actionButton.apply {
                    text = if (syncing) context.getText(R.string.sync_syncing_in_progress)
                    else context.getText(R.string.tab_drawer_fab_sync)
                    extend()
                    show()
                    icon = AppCompatResources.getDrawable(context, R.drawable.ic_fab_sync)
                    setOnClickListener {
                        // Notify the store observers (one of which is the SyncedTabsFeature), that
                        // a sync was requested.
                        if (!syncing) {
                            store.dispatch(TabsTrayAction.SyncNow)
                            syncedTabsInteractor.onRefresh()
                        }
                    }
                }
            }
        }
    }
}
