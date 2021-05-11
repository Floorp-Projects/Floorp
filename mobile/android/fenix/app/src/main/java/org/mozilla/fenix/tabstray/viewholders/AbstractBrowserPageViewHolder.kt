/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.widget.TextView
import androidx.annotation.CallSuper
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.browser.BrowserTrayList

/**
 * A shared view holder for browser tabs tray list.
 */
abstract class AbstractBrowserPageViewHolder(
    containerView: View,
    tabsTrayStore: TabsTrayStore,
    interactor: TabsTrayInteractor,
    private val currentTabIndex: Int
) : AbstractPageViewHolder(containerView) {

    protected val trayList: BrowserTrayList = itemView.findViewById(R.id.tray_list_item)
    private val emptyList: TextView = itemView.findViewById(R.id.tab_tray_empty_view)
    abstract val emptyStringText: String

    init {
        trayList.interactor = interactor
        trayList.tabsTrayStore = tabsTrayStore
        emptyList.text = emptyStringText
    }

    @CallSuper
    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    ) {
        adapter.registerAdapterDataObserver(OneTimeAdapterObserver(adapter) {
            trayList.scrollToPosition(currentTabIndex)
            updateTrayVisibility(adapter.itemCount)
        })
        trayList.layoutManager = layoutManager
        trayList.adapter = adapter
    }

    private fun updateTrayVisibility(size: Int) {
        if (size == 0) {
            trayList.visibility = GONE
            emptyList.visibility = VISIBLE
        } else {
            trayList.visibility = VISIBLE
            emptyList.visibility = GONE
        }
    }
}

/**
 * Observes the adapter and invokes the callback when data is first inserted.
 */
class OneTimeAdapterObserver(
    private val adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
    private val onAdapterReady: () -> Unit
) : RecyclerView.AdapterDataObserver() {
    override fun onItemRangeInserted(positionStart: Int, itemCount: Int) {
        onAdapterReady.invoke()
        adapter.unregisterAdapterDataObserver(this)
    }
}
