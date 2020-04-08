/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import org.mozilla.fenix.utils.Settings

interface SavedLoginsController {
    fun handleSort(sortingStrategy: SortingStrategy)
}

class DefaultSavedLoginsController(
    val store: SavedLoginsFragmentStore,
    val settings: Settings
) : SavedLoginsController {

    override fun handleSort(sortingStrategy: SortingStrategy) {
        store.dispatch(SavedLoginsFragmentAction.SortLogins(sortingStrategy))
        settings.savedLoginsSortingStrategy = sortingStrategy
    }
}
