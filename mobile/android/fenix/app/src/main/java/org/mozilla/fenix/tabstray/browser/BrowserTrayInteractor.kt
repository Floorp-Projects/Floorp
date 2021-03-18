/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.concept.tabstray.Tab
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.tabstray.TabsTrayInteractor

/**
 * For interacting with UI that extends from [BaseBrowserTrayList] and other browser tab tray views.
 */
interface BrowserTrayInteractor {

    /**
     * Select the tab.
     */
    fun onOpenTab(tab: Tab)

    /**
     * Close the tab.
     */
    fun onCloseTab(tab: Tab)

    /**
     * Enable or disable multi-select mode.
     */
    fun onMultiSelect(enabled: Boolean)
}

/**
 * A default implementation of [BrowserTrayInteractor].
 */
class DefaultBrowserTrayInteractor(
    private val trayInteractor: TabsTrayInteractor,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
    private val removeUseCases: TabsUseCases.RemoveTabUseCase
) : BrowserTrayInteractor {

    /**
     * See [BrowserTrayInteractor.onOpenTab].
     */
    override fun onOpenTab(tab: Tab) {
        selectTabUseCase.invoke(tab.id)
        trayInteractor.navigateToBrowser()
    }

    /**
     * See [BrowserTrayInteractor.onCloseTab].
     */
    override fun onCloseTab(tab: Tab) {
        removeUseCases.invoke(tab.id)
    }

    /**
     * See [BrowserTrayInteractor.onMultiSelect].
     */
    override fun onMultiSelect(enabled: Boolean) {
        // TODO https://github.com/mozilla-mobile/fenix/issues/18443
    }
}
