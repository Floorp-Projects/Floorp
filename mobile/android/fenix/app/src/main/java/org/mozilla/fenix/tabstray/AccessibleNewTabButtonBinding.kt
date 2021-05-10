/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.View
import android.widget.ImageButton
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.utils.Settings

/**
 * Do not show accessible new tab button when accessibility service is disabled
 *
 * This binding is coupled with [FloatingActionButtonBinding].
 * When [FloatingActionButtonBinding] is visible this should not be visible
 */
@OptIn(ExperimentalCoroutinesApi::class)
class AccessibleNewTabButtonBinding(
    private val store: TabsTrayStore,
    private val settings: Settings,
    private val newTabButton: ImageButton,
    private val browserTrayInteractor: BrowserTrayInteractor
) : AbstractBinding<TabsTrayState>(store) {

    override fun start() {
        if (!settings.accessibilityServicesEnabled) {
            newTabButton.visibility = View.GONE
            return
        }
        super.start()
    }

    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it }
            .ifAnyChanged { state ->
                arrayOf(
                    state.selectedPage,
                    state.syncing
                )
            }
            .collect { state ->
                setAccessibleNewTabButton(state.selectedPage, state.syncing)
            }
    }

    private fun setAccessibleNewTabButton(selectedPage: Page, syncing: Boolean) {
        when (selectedPage) {
            Page.NormalTabs -> {
                newTabButton.apply {
                    visibility = View.VISIBLE
                    setImageResource(R.drawable.ic_new)
                    setOnClickListener {
                        browserTrayInteractor.onFabClicked(false)
                    }
                }
            }
            Page.PrivateTabs -> {
                newTabButton.apply {
                    visibility = View.VISIBLE
                    setImageResource(R.drawable.ic_new)
                    setOnClickListener {
                        browserTrayInteractor.onFabClicked(true)
                    }
                }
            }
            Page.SyncedTabs -> {
                newTabButton.apply {
                    visibility =
                        when (syncing) {
                            true -> View.GONE
                            false -> View.VISIBLE
                        }

                    setImageResource(R.drawable.ic_fab_sync)
                    setOnClickListener {
                        // Notify the store observers (one of which is the SyncedTabsFeature), that
                        // a sync was requested.
                        if (!syncing) {
                            store.dispatch(TabsTrayAction.SyncNow)
                        }
                    }
                }
            }
        }
    }
}
