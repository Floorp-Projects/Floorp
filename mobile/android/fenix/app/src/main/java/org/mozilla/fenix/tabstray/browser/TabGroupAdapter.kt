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
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore
import kotlin.math.max
import mozilla.components.concept.tabstray.Tab as TabsTrayTab
import mozilla.components.support.base.observer.Observable

typealias TrayObservable = Observable<TabsTray.Observer>

/**
 * The [ListAdapter] for displaying the list of search term tabs.
 *
 * @param context [Context] used for various platform interactions or accessing [Components]
 * @param browserTrayInteractor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
 * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
 * @param delegate [Observable]<[TabsTray.Observer]> for observing tabs tray changes. Defaults to [ObserverRegistry].
 */
@Suppress("TooManyFunctions")
class TabGroupAdapter(
    private val context: Context,
    private val browserTrayInteractor: BrowserTrayInteractor,
    private val store: TabsTrayStore,
    private val featureName: String,
    delegate: TrayObservable = ObserverRegistry()
) : ListAdapter<TabGroupAdapter.Group, TabGroupViewHolder>(DiffCallback), TabsTray, TrayObservable by delegate {

    // TODO use [List<TabSessionState>.toSearchGroup()]
    //  see https://github.com/mozilla-mobile/android-components/issues/11012
    data class Group(
        /**
         * A title for the tab group.
         */
        val title: String,

        /**
         * The list of tabs belonging to this tab group.
         */
        val tabs: List<TabsTrayTab>,

        /**
         * The last time tabs in this group was accessed.
         */
        val lastAccess: Long
    )

    /**
     * Tracks the selected tabs in multi-select mode.
     */
    var selectionHolder: SelectionHolder<TabsTrayTab>? = null

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
        holder.bind(group, this)
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
     * Creates a grouping of data classes for how groupings will be structured.
     */
    override fun updateTabs(tabs: Tabs) {
        val data = tabs.list.groupBy { it.searchTerm.lowercase() }

        val grouping = data.map { mapEntry ->
            val searchTerm = mapEntry.key.replaceFirstChar(Char::uppercase)
            val groupTabs = mapEntry.value
            val groupMax = groupTabs.fold(0L) { acc, tab ->
                max(tab.lastAccess, acc)
            }

            Group(
                title = searchTerm,
                tabs = groupTabs,
                lastAccess = groupMax
            )
        }.sortedBy { it.lastAccess }

        submitList(grouping)
    }

    /**
     * Not implemented; handled by nested [RecyclerView].
     */
    override fun isTabSelected(tabs: Tabs, position: Int): Boolean = false
    override fun onTabsChanged(position: Int, count: Int) = Unit
    override fun onTabsInserted(position: Int, count: Int) = Unit
    override fun onTabsMoved(fromPosition: Int, toPosition: Int) = Unit
    override fun onTabsRemoved(position: Int, count: Int) = Unit

    private object DiffCallback : DiffUtil.ItemCallback<Group>() {
        override fun areItemsTheSame(oldItem: Group, newItem: Group) = oldItem.title == newItem.title
        override fun areContentsTheSame(oldItem: Group, newItem: Group) = oldItem == newItem
    }
}

internal fun TabGroupAdapter.Group.containsTabId(tabId: String): Boolean {
    return tabs.firstOrNull { it.id == tabId } != null
}
