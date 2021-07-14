/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.recyclerview.widget.ConcatAdapter
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.R
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.defaultBrowserLayoutColumns

/**
 * View holder for the normal tabs tray list.
 */
class NormalBrowserPageViewHolder(
    containerView: View,
    private val store: TabsTrayStore,
    interactor: TabsTrayInteractor,
    currentTabIndex: Int
) : AbstractBrowserPageViewHolder(
    containerView,
    store,
    interactor,
    currentTabIndex
),
    SelectionHolder<Tab> {

    /**
     * Holds the list of selected tabs.
     *
     * Implementation notes: we do this here because we only want the normal tabs list to be able
     * to select tabs.
     */
    override val selectedItems: Set<Tab>
        get() = store.state.mode.selectedTabs

    override val emptyStringText: String
        get() = itemView.resources.getString(R.string.no_open_tabs_description)

    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>
    ) {
        val browserAdapter = (adapter as ConcatAdapter).browserAdapter
        browserAdapter.selectionHolder = this

        val number = containerView.context.defaultBrowserLayoutColumns
        val manager = GridLayoutManager(containerView.context, number).apply {
            spanSizeLookup = object : GridLayoutManager.SpanSizeLookup() {
                override fun getSpanSize(position: Int): Int {
                    return if (position >= browserAdapter.itemCount) {
                        number
                    } else {
                        1
                    }
                }
            }
        }

        super.bind(adapter, manager)
    }

    companion object {
        const val LAYOUT_ID = R.layout.normal_browser_tray_list
    }
}
