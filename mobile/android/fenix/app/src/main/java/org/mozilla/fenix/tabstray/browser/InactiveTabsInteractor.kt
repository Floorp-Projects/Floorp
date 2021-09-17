/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

interface InactiveTabsInteractor {
    fun onHeaderClicked(activated: Boolean)
}

class DefaultInactiveTabsInteractor(
    private val controller: InactiveTabsController
) : InactiveTabsInteractor {
    override fun onHeaderClicked(activated: Boolean) {
        controller.updateCardExpansion(activated)
    }
}

/**
 * An experimental state holder for [InactiveTabsAdapter] that lives at the application lifetime.
 *
 * TODO This should be replaced with the AppStore.
 */
object InactiveTabsState {
    var isExpanded = false
}
