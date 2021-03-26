/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.recyclerview.selection.SelectionPredicates
import androidx.recyclerview.selection.SelectionTracker
import androidx.recyclerview.selection.StorageStrategy
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.TabsDetailsLookup
import org.mozilla.fenix.tabstray.browser.TabsItemKeyProvider

/**
 * View holder for the normal tabs tray list.
 */
class NormalBrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : BaseBrowserTabViewHolder(containerView, interactor) {

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
