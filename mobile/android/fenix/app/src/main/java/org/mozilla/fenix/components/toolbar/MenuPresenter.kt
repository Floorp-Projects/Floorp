/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.View
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged

class MenuPresenter(
    private val menuToolbar: BrowserToolbar,
    private val store: BrowserStore,
    private val sessionId: String? = null
) : View.OnAttachStateChangeListener {

    private var scope: CoroutineScope? = null

    fun start() {
        menuToolbar.addOnAttachStateChangeListener(this)
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(sessionId) }
                .ifAnyChanged { tab ->
                    arrayOf(
                        tab.content.loading,
                        tab.content.canGoBack,
                        tab.content.canGoForward,
                        tab.content.webAppManifest
                    )
                }
                .collect {
                    invalidateActions()
                }
        }
    }

    fun stop() {
        scope?.cancel()
    }

    fun invalidateActions() {
        menuToolbar.invalidateActions()
    }

    override fun onViewDetachedFromWindow(v: View?) {
        menuToolbar.onStop()
    }

    override fun onViewAttachedToWindow(v: View?) {
        // no-op
    }
}
