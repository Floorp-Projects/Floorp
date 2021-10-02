/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.tabstray.ext.getNormalTrayTabs
import org.mozilla.fenix.tabstray.ext.getSearchTabGroups
import org.mozilla.fenix.utils.Settings

/**
 * A binding class to notify an observer to show a title if there is at least one tab available.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class TitleHeaderBinding(
    store: BrowserStore,
    private val settings: Settings,
    private val showHeader: (Boolean) -> Unit
) : AbstractBinding<BrowserState>(store) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        val groupsEnabled = settings.searchTermTabGroupsAreEnabled
        val inactiveEnabled = settings.inactiveTabsAreEnabled

        flow.map { it.getSearchTabGroups(groupsEnabled) to it.getNormalTrayTabs(groupsEnabled, inactiveEnabled) }
            .ifChanged()
            .collect { (groups, normalTrayTabs) ->
                if (groups.isEmpty() || normalTrayTabs.isEmpty()) {
                    showHeader(false)
                } else {
                    showHeader(true)
                }
            }
    }
}
