/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import androidx.core.view.isVisible
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.checkbox_item.view.*
import kotlinx.android.synthetic.main.tab_tray_item.view.*
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsAdapter
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.settings

class FenixTabsAdapter(
    private val context: Context,
    imageLoader: ImageLoader
) : TabsAdapter(
    viewHolderProvider = { parentView ->
        TabTrayViewHolder(
            LayoutInflater.from(context).inflate(
                if (context.settings().gridTabView) R.layout.tab_tray_grid_item else R.layout.tab_tray_item,
                parentView,
                false
            ),
            imageLoader
        )
    }
) {
    private lateinit var tabsList: RecyclerView
    var tabTrayInteractor: TabTrayInteractor? = null

    private val mode: TabTrayDialogFragmentState.Mode?
        get() = tabTrayInteractor?.onModeRequested()

    val selectedItems get() = mode?.selectedItems ?: setOf()

    var onTabsUpdated: (() -> Unit)? = null
    var tabCount = 0

    override fun updateTabs(tabs: Tabs) {
        super.updateTabs(tabs)
        onTabsUpdated?.invoke()
        tabCount = tabs.list.size
    }

    override fun onBindViewHolder(
        holder: TabViewHolder,
        position: Int,
        payloads: MutableList<Any>
    ) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
            return
        }

        holder.tab?.let { showCheckedIfSelected(it, holder.itemView) }
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        super.onBindViewHolder(holder, position)

        val isListTabView = context.settings().listTabView

        val itemIndex: Int
        val rowIndex: Int
        val columnIndex: Int

        if (isListTabView) {
            itemIndex = tabCount - position - 1
            rowIndex = itemIndex
            columnIndex = 1
        } else {
            val columnsCount = (tabsList.layoutManager as GridLayoutManager).spanCount
            itemIndex = position
            rowIndex = itemIndex / columnsCount
            columnIndex = itemIndex % columnsCount
        }

        (holder as TabTrayViewHolder).updateAccessibilityCollectionItemInfo(
            holder.itemView,
            rowIndex,
            columnIndex,
            selectedItems.contains(holder.tab)
        )

        holder.tab?.let { tab ->
            showCheckedIfSelected(tab, holder.itemView)

            val tabIsPrivate =
                context.components.core.sessionManager.findSessionById(tab.id)?.private == true
            if (!tabIsPrivate) {
                holder.itemView.setOnLongClickListener {
                    if (mode is TabTrayDialogFragmentState.Mode.Normal) {
                        context.metrics.track(Event.CollectionTabLongPressed)
                        tabTrayInteractor?.onAddSelectedTab(
                            tab
                        )
                    }
                    true
                }
            } else {
                holder.itemView.setOnLongClickListener(null)
            }

            holder.itemView.setOnClickListener {
                if (mode is TabTrayDialogFragmentState.Mode.MultiSelect) {
                    if (mode?.selectedItems?.contains(tab) == true) {
                        tabTrayInteractor?.onRemoveSelectedTab(tab = tab)
                    } else {
                        tabTrayInteractor?.onAddSelectedTab(tab = tab)
                    }
                } else {
                    tabTrayInteractor?.onOpenTab(tab = tab)
                }
            }
        }
    }

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        tabsList = recyclerView
    }

    private fun showCheckedIfSelected(tab: Tab, view: View) {
        val shouldBeChecked =
            mode is TabTrayDialogFragmentState.Mode.MultiSelect && selectedItems.contains(tab)
        view.selected_mask.isVisible = shouldBeChecked
        view.mozac_browser_tabstray_close.isVisible = mode is TabTrayDialogFragmentState.Mode.Normal
    }
}
