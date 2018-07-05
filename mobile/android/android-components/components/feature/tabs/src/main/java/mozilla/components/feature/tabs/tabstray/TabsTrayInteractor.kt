/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import kotlinx.coroutines.experimental.android.UI
import kotlinx.coroutines.experimental.delay
import kotlinx.coroutines.experimental.launch
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

        launch(UI) {
            delay(CLOSE_TABS_TRAY_DELAY_MS)

            closeTabsTray.invoke()
        }
    }

    override fun onTabClosed(session: Session) {
        removeTabUseCase.invoke(session)
    }

    companion object {
        /**
         * Delay when closing the tabs tray after a user interaction. We delay closing the tabs
         * tray so that the user has a chance seeing the state change inside the tabs tray before
         * we close it. Android uses 250ms for most of its animations (e.g.
         * Switch.THUMB_ANIMATION_DURATION).
         */
        private const val CLOSE_TABS_TRAY_DELAY_MS = 250L
    }
}
