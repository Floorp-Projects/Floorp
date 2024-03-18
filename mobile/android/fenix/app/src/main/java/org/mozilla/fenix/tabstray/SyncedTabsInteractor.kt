/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.storage.sync.Tab

/**
 * Interactor for responding to any actions on synced tabs in the tabs tray.
 */
interface SyncedTabsInteractor {
    /**
     * Invoked when the user clicks on a synced [Tab].
     *
     * @param tab The synced [Tab] that was clicked.
     */
    fun onSyncedTabClicked(tab: Tab)
}
