/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.privatemode.feature

import android.view.Window
import android.view.WindowManager.LayoutParams.FLAG_SECURE
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Prevents screenshots and screen recordings in private tabs.
 *
 * @param isSecure Returns true if the session should have [FLAG_SECURE] set.
 * @param clearFlagOnStop Used to keep [FLAG_SECURE] enabled or not when calling [stop].
 * Can be overriden to customize when the secure flag is set.
 */
class SecureWindowFeature(
    private val window: Window,
    private val store: BrowserStore,
    private val customTabId: String? = null,
    private val isSecure: (SessionState) -> Boolean = { it.content.private },
    private val clearFlagOnStop: Boolean = true,
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(customTabId) }
                .map { isSecure(it) }
                .ifChanged()
                .collect { isSecure ->
                    if (isSecure) {
                        window.addFlags(FLAG_SECURE)
                    } else {
                        window.clearFlags(FLAG_SECURE)
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
        if (clearFlagOnStop) {
            window.clearFlags(FLAG_SECURE)
        }
    }
}
