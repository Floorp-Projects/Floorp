/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import android.content.Context
import android.util.AttributeSet
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.fragment.app.findFragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import kotlinx.android.synthetic.main.component_sync_tabs_tray_layout.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.SyncedTabsFeature
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.sync.SyncedTabsAdapter
import org.mozilla.fenix.sync.SyncedTabsTitleDecoration
import org.mozilla.fenix.sync.ext.toAdapterItem
import org.mozilla.fenix.sync.ext.toStringRes
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayFragment
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.view.LifecycleViewProvider

class SyncedTabsTrayLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : ConstraintLayout(context, attrs, defStyleAttr), SyncedTabsView,
    Observable<SyncedTabsView.Listener> by ObserverRegistry() {

    private val lifecycleProvider = LifecycleViewProvider(this)
    private val coroutineScope = CoroutineScope(Dispatchers.Main)

    private val syncedTabsFeature by lazy {
        SyncedTabsFeature(
            context = context,
            storage = context.components.backgroundServices.syncedTabsStorage,
            accountManager = context.components.backgroundServices.accountManager,
            view = this,
            lifecycleOwner = lifecycleProvider,
            onTabClicked = {
                // We can ignore this callback here because we're not connecting the adapter
                // back to the feature. This works fine in other features, but passing the listener
                // to other components in this case is annoying.
            }
        )
    }

    private val syncButtonBinding by lazy {
        SyncButtonBinding(tabsTrayStore) {
            listener?.onRefresh()
        }
    }

    lateinit var tabsTrayStore: TabsTrayStore

    override var listener: SyncedTabsView.Listener? = null

    override fun onFinishInflate() {
        synced_tabs_list.addItemDecoration(SyncedTabsTitleDecoration(context))

        super.onFinishInflate()
    }

    override fun displaySyncedTabs(syncedTabs: List<SyncedDeviceTabs>) {
        coroutineScope.launch {
            (synced_tabs_list.adapter as SyncedTabsAdapter).updateData(syncedTabs)
        }
    }

    override fun onError(error: SyncedTabsView.ErrorType) {
        coroutineScope.launch {
            // We may still be displaying a "loading" spinner, hide it.
            stopLoading()

            val navController: NavController? = try {
                findFragment<TabsTrayFragment>().findNavController()
            } catch (exception: IllegalStateException) {
                null
            }

            val descriptionResId = error.toStringRes()
            val errorItem = error.toAdapterItem(descriptionResId, navController)

            val errorList: List<SyncedTabsAdapter.AdapterItem> = listOf(errorItem)
            (synced_tabs_list.adapter as SyncedTabsAdapter).submitList(errorList)
        }
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        syncedTabsFeature.start()
        syncButtonBinding.start()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        syncedTabsFeature.stop()
        syncButtonBinding.stop()

        coroutineScope.cancel()
    }

    override fun stopLoading() {
        tabsTrayStore.dispatch(TabsTrayAction.SyncCompleted)
    }

    /**
     * Do nothing; the UI is handled with FloatingActionButtonBinding.
     */
    override fun startLoading() = Unit
}
