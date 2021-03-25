/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.View
import androidx.annotation.CallSuper
import androidx.recyclerview.selection.SelectionPredicates
import androidx.recyclerview.selection.SelectionTracker
import androidx.recyclerview.selection.StableIdKeyProvider
import androidx.recyclerview.selection.StorageStrategy
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BaseBrowserTrayList
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.TabsDetailsLookup

/**
 * Base [RecyclerView.ViewHolder] for [TrayPagerAdapter] items.
 */
abstract class TrayViewHolder constructor(
    override val containerView: View
) : RecyclerView.ViewHolder(containerView), LayoutContainer {

    abstract fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    )
}

abstract class BrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : TrayViewHolder(containerView) {

    protected val trayList: BaseBrowserTrayList = itemView.findViewById(R.id.tray_list_item)

    init {
        trayList.interactor = interactor
    }

    @CallSuper
    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    ) {
        trayList.layoutManager = layoutManager
        trayList.adapter = adapter
    }
}

class NormalBrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : BrowserTabViewHolder(containerView, interactor) {

    private lateinit var selectionTracker: SelectionTracker<Long>

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    ) {
        super.bind(adapter, layoutManager)

        selectionTracker = SelectionTracker.Builder(
            "mySelection",
            trayList,
            TabsItemKeyProvider(trayList),
            TabsDetailsLookup(trayList),
            StorageStrategy.createLongStorage()
        ).withSelectionPredicate(
            SelectionPredicates.createSelectAnything()
        ).build()

        (adapter as BrowserTabsAdapter).tracker = selectionTracker

        selectionTracker.addObserver(object : SelectionTracker.SelectionObserver<Long>() {
            override fun onItemStateChanged(key: Long, selected: Boolean) {
                // TODO Do nothing for now; remove in a future patch if needed.
            }
        })
    }

    companion object {
        const val LAYOUT_ID = R.layout.normal_browser_tray_list
    }
}

class PrivateBrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : BrowserTabViewHolder(containerView, interactor) {
    companion object {
        const val LAYOUT_ID = R.layout.private_browser_tray_list
    }
}
