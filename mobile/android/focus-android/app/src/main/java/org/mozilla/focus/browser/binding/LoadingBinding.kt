/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.binding

import android.view.View
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.widget.AnimatedProgressBar

private const val INITIAL_PROGRESS = 5

class LoadingBinding(
    store: BrowserStore,
    private val tabId: String,
    private val progressBar: AnimatedProgressBar
) : AbstractBinding(store) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
            .ifChanged { tab -> tab.content.loading }
            .collect { tab -> onLoadingStateChanged(tab) }
    }

    private fun onLoadingStateChanged(tab: SessionState) {
        if (tab.content.loading) {
            progressBar.progress = INITIAL_PROGRESS
            progressBar.visibility = View.VISIBLE
        } else {
            if (progressBar.visibility == View.VISIBLE) {
                progressBar.visibility = View.GONE
            }
        }
    }
}
