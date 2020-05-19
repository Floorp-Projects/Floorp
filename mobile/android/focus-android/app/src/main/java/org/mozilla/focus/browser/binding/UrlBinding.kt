/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.binding

import android.widget.TextView
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.utils.UrlUtils

class UrlBinding(
    store: BrowserStore,
    val tabId: String,
    val urlView: TextView
) : AbstractBinding(store) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
            .map { tab -> tab.content.url }
            .ifChanged()
            .collect { url -> onUrlChanged(url) }
    }

    private fun onUrlChanged(url: String) {
        urlView.text = UrlUtils.stripUserInfo(url)
    }
}
