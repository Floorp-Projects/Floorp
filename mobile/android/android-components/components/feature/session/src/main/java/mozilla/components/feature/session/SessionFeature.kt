/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.session.engine.EngineViewPresenter
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler

/**
 * Feature implementation for connecting the engine module with the session module.
 */
class SessionFeature(
    private val store: BrowserStore,
    private val goBackUseCase: SessionUseCases.GoBackUseCase,
    private val engineView: EngineView,
    private val tabId: String? = null,
) : LifecycleAwareFeature, UserInteractionHandler {
    internal val presenter = EngineViewPresenter(store, engineView, tabId)

    /**
     * Start feature: App is in the foreground.
     */
    override fun start() {
        presenter.start()
    }

    /**
     * Handler for back pressed events in activities that use this feature.
     *
     * @return true if the event was handled, otherwise false.
     */
    override fun onBackPressed(): Boolean {
        val tab = store.state.findTabOrCustomTabOrSelectedTab(tabId)

        if (engineView.canClearSelection()) {
            engineView.clearSelection()
            return true
        } else if (tab?.content?.canGoBack == true) {
            goBackUseCase(tab.id)
            return true
        }

        return false
    }

    /**
     * Stop feature: App is in the background.
     */
    override fun stop() {
        presenter.stop()
    }

    /**
     * Stops the feature from rendering sessions on the [EngineView] (until explicitly started again)
     * and releases an already rendering session from the [EngineView].
     */
    fun release() {
        // Once we fully migrated to BrowserStore we may be able to get rid of the need for cleanup().
        // See https://github.com/mozilla-mobile/android-components/issues/7657
        presenter.stop()
    }
}
