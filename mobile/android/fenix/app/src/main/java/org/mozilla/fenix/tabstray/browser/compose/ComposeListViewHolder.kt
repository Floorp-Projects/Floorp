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
import org.mozilla.fenix.compose.tabstray.TabListItem
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * A Compose ViewHolder implementation for "tab" items with list layout.
 *
 * @param interactor [TabsTrayInteractor] handling tabs interactions in a tab tray.
 * @param tabsTrayStore [TabsTrayStore] containing the complete state of tabs tray and methods to update that.
 * @param composeItemView that displays a "tab".
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 */
class ComposeListViewHolder(
    private val interactor: TabsTrayInteractor,
    private val tabsTrayStore: TabsTrayStore,
    composeItemView: ComposeView,
    private val featureName: String,
    viewLifecycleOwner: LifecycleOwner,
) : ComposeAbstractTabViewHolder(composeItemView, viewLifecycleOwner) {

    private var delegate: TabsTray.Delegate? = null

    override var tab: TabSessionState? = null
    private val isMultiSelectionSelected = MutableStateFlow(false)
    private val isSelectedTab = MutableStateFlow(false)

    override fun bind(
        tab: TabSessionState,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        delegate: TabsTray.Delegate,
    ) {
        this.tab = tab
        this.delegate = delegate
        isSelectedTab.value = isSelected
        bind(tab)
    }

    override fun updateSelectedTabIndicator(showAsSelected: Boolean) {
        isSelectedTab.value = showAsSelected
    }

    override fun showTabIsMultiSelectEnabled(selectedMaskView: View?, isSelected: Boolean) {
        isMultiSelectionSelected.value = isSelected
    }

    private fun onCloseClicked(tab: TabSessionState) {
        delegate?.onTabClosed(tab, featureName)
    }

    private fun onClick(tab: TabSessionState) {
        interactor.onTabSelected(tab, featureName)
    }

    private fun onLongClick(tab: TabSessionState) {
        interactor.onTabLongClicked(tab)
    }

    @Composable
    override fun Content(tab: TabSessionState) {
        val multiSelectionEnabled = tabsTrayStore.observeAsComposableState {
                state ->
            state.mode is TabsTrayState.Mode.Select
        }.value ?: false
        val isSelectedTabState by isSelectedTab.collectAsState()
        val multiSelectionSelected by isMultiSelectionSelected.collectAsState()

        TabListItem(
            tab = tab,
            isSelected = isSelectedTabState,
            multiSelectionEnabled = multiSelectionEnabled,
            multiSelectionSelected = multiSelectionSelected,
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
