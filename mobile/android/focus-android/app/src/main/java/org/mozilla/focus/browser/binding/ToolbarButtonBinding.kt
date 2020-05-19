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
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

/**
 * Binding that will update the navigation buttons (only on tablets) based on the navigation state
 * in [BrowserStore].
 */
class ToolbarButtonBinding(
    store: BrowserStore,
    private val tabId: String,
    private val forwardButton: View,
    private val backButton: View,
    private val refreshButton: View,
    private val stopButton: View
) : AbstractBinding(store) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
            .ifAnyChanged { tab -> arrayOf(tab.content.canGoBack, tab.content.canGoForward, tab.content.loading) }
            .collect { tab ->
                onTabChanged(tab.content.canGoBack, tab.content.canGoForward, tab.content.loading)
            }
    }

    @Suppress("MagicNumber")
    private fun onTabChanged(canGoBack: Boolean, canGoForward: Boolean, isLoading: Boolean) {
        forwardButton.isEnabled = canGoForward
        forwardButton.alpha = if (canGoForward) 1.0f else 0.5f
        backButton.isEnabled = canGoBack
        backButton.alpha = if (canGoBack) 1.0f else 0.5f

        refreshButton.visibility = if (isLoading) View.GONE else View.VISIBLE
        stopButton.visibility = if (isLoading) View.VISIBLE else View.GONE
    }
}
