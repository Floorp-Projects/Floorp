/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.interactor

import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.controller.SyncedTabsController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * An interactor that handles events from [SyncedTabsView.Listener].
 */
interface SyncedTabsInteractor : SyncedTabsView.Listener, LifecycleAwareFeature {
    val controller: SyncedTabsController
    val view: SyncedTabsView
    val tabClicked: (Tab) -> Unit
}
