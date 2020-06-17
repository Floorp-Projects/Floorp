/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

interface TabTrayInteractor {
    fun onNewTabTapped(private: Boolean)
    fun onTabTrayDismissed()
    fun onShareTabsClicked(private: Boolean)
    fun onSaveToCollectionClicked()
    fun onCloseAllTabsClicked(private: Boolean)
}

/**
 * Interactor for the tab tray fragment.
 */
class TabTrayFragmentInteractor(private val controller: TabTrayController) : TabTrayInteractor {
    override fun onNewTabTapped(private: Boolean) {
        controller.onNewTabTapped(private)
    }

    override fun onTabTrayDismissed() {
        controller.onTabTrayDismissed()
    }

    override fun onShareTabsClicked(private: Boolean) {
        controller.onShareTabsClicked(private)
    }

    override fun onSaveToCollectionClicked() {
        controller.onSaveToCollectionClicked()
    }

    override fun onCloseAllTabsClicked(private: Boolean) {
        controller.onCloseAllTabsClicked(private)
    }
}
