/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * A tabs observer that informs [showHeader] if an "Other tabs" title should be displayed in the tray.
 */
class OtherHeaderBinding(
    store: TabsTrayStore,
    private val showHeader: (Boolean) -> Unit
) : AbstractBinding<TabsTrayState>(store) {
    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.ifAnyChanged { arrayOf(it.normalTabs, it.searchTermGroups) }
            .collect {
                if (it.searchTermGroups.isNotEmpty() && it.normalTabs.isNotEmpty()) {
                    showHeader(true)
                } else {
                    showHeader(false)
                }
            }
    }
}
