/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.presenter

import mozilla.components.feature.syncedtabs.controller.SyncedTabsController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * A presenter that orchestrates the [FxaAccountManager] being in the correct state to request remote tabs from the
 * [SyncedTabsController] or notifies [SyncedTabsView.onError] otherwise.
 */
interface SyncedTabsPresenter : LifecycleAwareFeature {
    val controller: SyncedTabsController
    val accountManager: FxaAccountManager
    val view: SyncedTabsView
}
