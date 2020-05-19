package org.mozilla.focus.browser.binding

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.widget.AnimatedProgressBar

class ProgressBinding(
    store: BrowserStore,
    private val tabId: String?,
    private val progressView: AnimatedProgressBar
) : AbstractBinding(store) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
            .map { tab -> tab.content.progress }
            .ifChanged()
            .collect { progress -> onProgressChanged(progress) }
    }

    private fun onProgressChanged(progress: Int) {
        progressView.progress = progress
    }
}
