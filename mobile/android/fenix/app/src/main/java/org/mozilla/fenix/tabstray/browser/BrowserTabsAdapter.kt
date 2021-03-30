/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.ViewGroup
import androidx.recyclerview.selection.SelectionTracker
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.GridLayoutManager
import kotlinx.android.synthetic.main.tab_tray_item.view.*
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.tabstray.TabsTrayViewHolder

/**
 * A [RecyclerView.Adapter] for browser tabs.
 */
class BrowserTabsAdapter(
    private val context: Context,
    private val interactor: BrowserTrayInteractor,
    private val layoutManager: (() -> GridLayoutManager)? = null,
    delegate: Observable<TabsTray.Observer> = ObserverRegistry()
) : TabsAdapter<TabsTrayViewHolder>(delegate) {

    /**
     * The layout types for the tabs.
     */
    enum class ViewType {
        LIST,
        GRID
    }

    /**
     * Tracks the selected tabs in multi-select mode.
     */
    var tracker: SelectionTracker<Long>? = null

    private val imageLoader = ThumbnailLoader(context.components.core.thumbnailStorage)

    override fun getItemViewType(position: Int): Int {
        return if (context.settings().gridTabView) {
            ViewType.GRID.ordinal
        } else {
            ViewType.LIST.ordinal
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabsTrayViewHolder {
        return when (viewType) {
            ViewType.GRID.ordinal -> TabsTrayGridViewHolder(parent, imageLoader, interactor)
            else -> TabsTrayListViewHolder(parent, imageLoader, interactor)
        }
    }

    override fun onBindViewHolder(holder: TabsTrayViewHolder, position: Int) {
        super.onBindViewHolder(holder, position)

        holder.tab?.let { tab ->
            holder.itemView.setOnClickListener {
                interactor.onOpenTab(tab)
            }

            holder.itemView.mozac_browser_tabstray_close.setOnClickListener {
                interactor.onCloseTab(tab)
            }

            tracker?.let {
                holder.showTabIsMultiSelectEnabled(it.isSelected(getItemId(position)))
            }
        }
    }
}
