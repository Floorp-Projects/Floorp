/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.TabsTrayFabInteractor

/**
 * A binding that show a FAB in tab tray used to open a new tab.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class FloatingActionButtonBinding(
    store: TabsTrayStore,
    private val actionButton: ExtendedFloatingActionButton,
    private val interactor: TabsTrayFabInteractor,
    private val isSignedIn: Boolean,
) : AbstractBinding<TabsTrayState>(store) {

    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it }
            .ifAnyChanged { state ->
                arrayOf(
                    state.selectedPage,
                    state.syncing,
                )
            }
            .collect { state ->
                setFab(state.selectedPage, state.syncing)
            }
    }

    private fun setFab(selectedPage: Page, syncing: Boolean) {
        when (selectedPage) {
            Page.NormalTabs -> {
                actionButton.apply {
                    shrink()
                    show()
                    contentDescription = context.getString(R.string.add_tab)
                    setIconResource(R.drawable.ic_new)
                    setOnClickListener {
                        interactor.onNormalTabsFabClicked()
                    }
                }
            }
            Page.PrivateTabs -> {
                actionButton.apply {
                    setText(R.string.tab_drawer_fab_content)
                    extend()
                    show()
                    contentDescription = context.getString(R.string.add_private_tab)
                    setIconResource(R.drawable.ic_new)
                    setOnClickListener {
                        interactor.onPrivateTabsFabClicked()
                    }
                }
            }
            Page.SyncedTabs -> {
                if (isSignedIn) {
                    actionButton.apply {
                        setText(
                            when (syncing) {
                                true -> R.string.sync_syncing_in_progress
                                false -> R.string.tab_drawer_fab_sync
                            },
                        )
                        contentDescription =
                            context.getString(R.string.resync_button_content_description)
                        extend()
                        show()
                        setIconResource(R.drawable.ic_fab_sync)
                        setOnClickListener {
                            interactor.onSyncedTabsFabClicked()
                        }
                    }
                } else {
                    actionButton.hide()
                }
            }
        }
    }
}
