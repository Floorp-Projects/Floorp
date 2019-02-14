/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import mozilla.components.browser.session.Session
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.TabsUseCases

/**
 * Interactor for a tabs tray component. Subscribes to the tabs tray and invokes use cases to update
 * the session manager.
 */
class TabsTrayInteractor(
    private val tabsTray: TabsTray,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
    private val removeTabUseCase: TabsUseCases.RemoveTabUseCase,
    private val closeTabsTray: () -> Unit
) : TabsTray.Observer {

    fun start() {
        tabsTray.register(this)
    }

    fun stop() {
        tabsTray.unregister(this)
    }

    override fun onTabSelected(session: Session) {
        selectTabUseCase.invoke(session)
        closeTabsTray.invoke()
    }

    override fun onTabClosed(session: Session) {
        removeTabUseCase.invoke(session)
    }
}
