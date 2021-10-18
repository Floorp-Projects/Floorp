/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import TabGroupViewHolder
import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.HORIZONTAL
import androidx.recyclerview.widget.RecyclerView.VERTICAL
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * The [ListAdapter] for displaying the list of search term tabs.
 *
 * @param context [Context] used for various platform interactions or accessing [Components]
 * @param browserTrayInteractor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 */
@Suppress("TooManyFunctions")
class TabGroupAdapter(
    private val context: Context,
    private val browserTrayInteractor: BrowserTrayInteractor,
    private val store: TabsTrayStore,
    override val featureName: String,
) : ListAdapter<TabGroup, TabGroupViewHolder>(DiffCallback), TabsTray, FeatureNameHolder {

    /**
     * Tracks the selected tabs in multi-select mode.
     */
    var selectionHolder: SelectionHolder<TabSessionState>? = null

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabGroupViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when {
            context.components.settings.gridTabView -> {
                TabGroupViewHolder(view, HORIZONTAL, browserTrayInteractor, store, selectionHolder)
            }
            else -> {
                TabGroupViewHolder(view, VERTICAL, browserTrayInteractor, store, selectionHolder)
            }
        }
    }

    override fun onBindViewHolder(holder: TabGroupViewHolder, position: Int) {
        val group = getItem(position)
        holder.bind(group)
    }

    override fun getItemViewType(position: Int) = TabGroupViewHolder.LAYOUT_ID

    /**
     * Notify the nested [RecyclerView] when this view has been attached.
     */
    override fun onViewAttachedToWindow(holder: TabGroupViewHolder) {
        holder.rebind()
    }

    /**
     * Notify the nested [RecyclerView] when this view has been detached.
     */
    override fun onViewDetachedFromWindow(holder: TabGroupViewHolder) {
        holder.unbind()
    }

    /**
     * Not implemented; implementation is handled [List<Tab>.toSearchGroups]
     */
    override fun updateTabs(tabs: List<TabSessionState>, selectedTabId: String?) =
        throw UnsupportedOperationException("Use submitList instead.")

    private object DiffCallback : DiffUtil.ItemCallback<TabGroup>() {
        override fun areItemsTheSame(oldItem: TabGroup, newItem: TabGroup) = oldItem.searchTerm == newItem.searchTerm
        override fun areContentsTheSame(oldItem: TabGroup, newItem: TabGroup) = oldItem == newItem
    }
}

internal fun TabGroup.containsTabId(tabId: String): Boolean {
    return tabs.firstOrNull { it.id == tabId } != null
}
