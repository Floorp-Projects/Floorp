/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.binding

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.widget.FloatingEraseButton
import org.mozilla.focus.widget.FloatingSessionsButton

/**
 * Binding for updating views with the current number of open tabs.
 */
class TabCountBinding(
    store: BrowserStore,
    private val eraseButton: FloatingEraseButton,
    private val tabsButton: FloatingSessionsButton
) : AbstractBinding<BrowserState>(store) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.map { state -> state.privateTabs.size }
            .ifChanged()
            .collect { tabs -> onTabCountChanged(tabs) }
    }

    private fun onTabCountChanged(tabs: Int) {
        tabsButton.updateSessionsCount(tabs)
        eraseButton.updateSessionsCount(tabs)
    }
}
