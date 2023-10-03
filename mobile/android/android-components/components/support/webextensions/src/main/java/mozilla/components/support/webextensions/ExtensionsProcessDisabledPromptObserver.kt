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
 * Feature implementation that shows a prompt when the extensions process spawning has been
 * disabled due to the restart threshold being met.
 *
 * @property store the application's [BrowserStore].
 * @property onShowExtensionsProcessDisabledPrompt a callback invoked when the application should open a
 * prompt.
 */
open class ExtensionsProcessDisabledPromptObserver(
    private val store: BrowserStore,
    private val onShowExtensionsProcessDisabledPrompt: () -> Unit = { },
) : LifecycleAwareFeature {
    private var promptScope: CoroutineScope? = null

    override fun start() {
        promptScope = store.flowScoped { flow ->
            flow.distinctUntilChangedBy { it.showExtensionsProcessDisabledPrompt }
                .collect { state ->
                    if (state.showExtensionsProcessDisabledPrompt) {
                        // There should only be one active dialog to the user when the extensions
                        // process spawning is disabled.
                        onShowExtensionsProcessDisabledPrompt()
                    }
                }
        }
    }

    override fun stop() {
        promptScope?.cancel()
        promptScope = null
    }
}
