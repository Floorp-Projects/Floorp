/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest

/**
 * Feature used to handle web content settings from manifest file.
 */
class WebAppContentFeature(
    private val store: BrowserStore,
    private val tabId: String? = null,
    private val manifest: WebAppManifest,
) : DefaultLifecycleObserver {

    override fun onCreate(owner: LifecycleOwner) {
        setDisplayMode(manifest.display)
    }

    private fun setDisplayMode(display: WebAppManifest.DisplayMode) {
        val tab = store.state.findTabOrCustomTabOrSelectedTab(tabId)
        tab?.engineState?.engineSession?.setDisplayMode(display)
    }
}
