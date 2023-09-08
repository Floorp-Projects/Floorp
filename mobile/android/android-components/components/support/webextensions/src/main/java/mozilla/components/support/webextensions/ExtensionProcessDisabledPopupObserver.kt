/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.webextensions

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Feature implementation that shows a popup when the extensions process spawning has been
 * disabled due to the restart threshold being met.
 *
 * @property store the application's [BrowserStore].
 * @property onShowExtensionProcessDisabledPopup a callback invoked when the application should open a
 * popup.
 */
open class ExtensionProcessDisabledPopupObserver(
    private val store: BrowserStore,
    private val onShowExtensionProcessDisabledPopup: () -> Unit = { },
) : LifecycleAwareFeature {
    private var popupScope: CoroutineScope? = null

    override fun start() {
        popupScope = store.flowScoped { flow ->
            flow.distinctUntilChangedBy { it.showExtensionProcessDisabledPopup }
                .collect { state ->
                    if (state.showExtensionProcessDisabledPopup) {
                        // There should only be one active dialog to the user when the extensions
                        // process spawning is disabled.
                        onShowExtensionProcessDisabledPopup()
                    }
                }
        }
    }

    override fun stop() {
        popupScope?.cancel()
        popupScope = null
    }
}
