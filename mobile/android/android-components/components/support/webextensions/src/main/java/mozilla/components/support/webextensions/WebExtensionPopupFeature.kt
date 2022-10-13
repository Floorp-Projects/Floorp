/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature implementation that opens popups for web extensions browser actions.
 *
 * @property store the application's [BrowserStore].
 * @property onOpenPopup a callback invoked when the application should open a
 * popup. This is a lambda accepting the [WebExtensionState] of the extension
 * that wants to open a popup.
 */
class WebExtensionPopupFeature(
    private val store: BrowserStore,
    private val onOpenPopup: (WebExtensionState) -> Unit = { },
) : LifecycleAwareFeature {
    private var popupScope: CoroutineScope? = null

    override fun start() {
        popupScope = store.flowScoped { flow ->
            flow.ifChanged { it.extensions }
                .map { it.extensions.filterValues { extension -> extension.popupSession != null } }
                .ifChanged()
                .collect { extensionStates ->
                    if (extensionStates.values.isNotEmpty()) {
                        // We currently limit to one active popup session at a time
                        onOpenPopup(extensionStates.values.first())
                    }
                }
        }
    }

    override fun stop() {
        popupScope?.cancel()
    }
}
