/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.syncedhistory

import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.library.LibraryPageFragment
import org.mozilla.fenix.library.history.History

/**
 * A screen displaying history items that were opened on other devices, not local.
 */
class SyncedHistoryFragment : LibraryPageFragment<History>(), UserInteractionHandler {

    override fun onBackPressed(): Boolean {
        return false
    }

    override val selectedItems: Set<History>
        get() = setOf()
}
