/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.view.View
import androidx.fragment.app.FragmentManager.findFragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.sync.ListenerDelegate
import org.mozilla.fenix.sync.SyncedTabsAdapter
import org.mozilla.fenix.sync.ext.toAdapterList
import org.mozilla.fenix.sync.ext.toAdapterItem
import org.mozilla.fenix.sync.ext.toStringRes
import kotlin.coroutines.CoroutineContext

class SyncedTabsController(
    private val view: View,
    coroutineContext: CoroutineContext = Dispatchers.Main
) : SyncedTabsView {
    override var listener: SyncedTabsView.Listener? = null

    val adapter = SyncedTabsAdapter(ListenerDelegate { listener })

    private val scope: CoroutineScope = CoroutineScope(coroutineContext)

    override fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>) {
        scope.launch {
            val tabsList = listOf(SyncedTabsAdapter.AdapterItem.Title) + syncedTabs.toAdapterList()
            // Reverse layout for TabTrayView which does things backwards.
            adapter.submitList(tabsList.reversed())
        }
    }

    override fun onError(error: SyncedTabsView.ErrorType) {
        scope.launch {
            val navController: NavController? = try {
                findFragment<TabTrayDialogFragment>(view).findNavController()
            } catch (exception: IllegalStateException) {
                null
            }

            val descriptionResId = error.toStringRes()
            val errorItem = error.toAdapterItem(descriptionResId, navController)

            adapter.submitList(listOf(errorItem))
        }
    }
}
