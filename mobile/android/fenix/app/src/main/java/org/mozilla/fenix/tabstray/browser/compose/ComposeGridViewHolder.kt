/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser.compose

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.flow.MutableStateFlow
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.compose.tabstray.TabGridItem
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * A Compose ViewHolder implementation for "tab" items with grid layout.
 *
 * @param interactor [TabsTrayInteractor] handling tabs interactions in a tab tray.
 * @param store [TabsTrayStore] containing the complete state of tabs tray and methods to update that.
 * @param selectionHolder [SelectionHolder]<[TabSessionState]> for helping with selecting
 * any number of displayed [TabSessionState]s.
 * @param composeItemView that displays a "tab".
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 */
class ComposeGridViewHolder(
    private val interactor: TabsTrayInteractor,
    private val store: TabsTrayStore,
    private val selectionHolder: SelectionHolder<TabSessionState>? = null,
    composeItemView: ComposeView,
    private val featureName: String,
    viewLifecycleOwner: LifecycleOwner,
) : ComposeAbstractTabViewHolder(composeItemView, viewLifecycleOwner) {

    override var tab: TabSessionState? = null
    private var isMultiSelectionSelectedState = MutableStateFlow(false)
    private var isSelectedTabState = MutableStateFlow(false)

    override fun bind(
        tab: TabSessionState,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        delegate: TabsTray.Delegate,
    ) {
        this.tab = tab
        isSelectedTabState.value = isSelected
        bind(tab)
    }

    override fun updateSelectedTabIndicator(showAsSelected: Boolean) {
        isSelectedTabState.value = showAsSelected
    }

    override fun showTabIsMultiSelectEnabled(selectedMaskView: View?, isSelected: Boolean) {
        isMultiSelectionSelectedState.value = isSelected
    }

    private fun onCloseClicked(tab: TabSessionState) {
        interactor.onTabClosed(tab, featureName)
    }

    private fun onClick(tab: TabSessionState) {
        val holder = selectionHolder
        if (holder != null) {
            interactor.onMultiSelectClicked(tab, holder, featureName)
        } else {
            interactor.onTabSelected(tab, featureName)
        }
    }

    private fun onLongClick(tab: TabSessionState) {
        val holder = selectionHolder ?: return
        interactor.onTabLongClicked(tab, holder)
    }

    @Composable
    override fun Content(tab: TabSessionState) {
        val multiSelectionEnabled = store.observeAsComposableState { state ->
            state.mode is TabsTrayState.Mode.Select
        }.value ?: false
        val isSelectedTab by isSelectedTabState.collectAsState()
        val isMultiSelectionSelected by isMultiSelectionSelectedState.collectAsState()

        TabGridItem(
            tab = tab,
            isSelected = isSelectedTab,
            multiSelectionEnabled = multiSelectionEnabled,
            multiSelectionSelected = isMultiSelectionSelected,
            onCloseClick = ::onCloseClicked,
            onMediaClick = interactor::onMediaClicked,
            onClick = ::onClick,
            onLongClick = ::onLongClick,
        )
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
