/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.GridLayoutManager
import kotlinx.android.synthetic.main.tab_tray_item.view.*
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.tabtray.TabTrayViewHolder

/**
 * A [RecyclerView.Adapter] for browser tabs.
 */
class BrowserTabsAdapter(
    private val context: Context,
    private val interactor: BrowserTrayInteractor,
    private val layoutManager: (() -> GridLayoutManager)? = null,
    delegate: Observable<TabsTray.Observer> = ObserverRegistry()
) : TabsAdapter(delegate) {

    /**
     * The layout types for the tabs.
     */
    enum class ViewType {
        LIST,
        GRID
    }

    private val imageLoader = ThumbnailLoader(context.components.core.thumbnailStorage)

    override fun getItemViewType(position: Int): Int {
        return if (context.settings().gridTabView) {
            // ViewType.GRID.ordinal
            R.layout.tab_tray_grid_item
        } else {
            // ViewType.LIST.ordinal
            R.layout.tab_tray_item
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TabViewHolder {
        // TODO make this into separate view holders for each layout
        // For this, we need to separate the TabTrayViewHolder as well.
        // See https://github.com/mozilla-mobile/fenix/issues/18535
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return TabTrayViewHolder(view, imageLoader)
    }

    override fun onBindViewHolder(holder: TabViewHolder, position: Int) {
        super.onBindViewHolder(holder, position)

        holder.tab?.let { tab ->
            if (!tab.private) {
                holder.itemView.setOnLongClickListener {
                    interactor.onMultiSelect(true)
                    true
                }
            } else {
                holder.itemView.setOnLongClickListener(null)
            }

            holder.itemView.setOnClickListener {
                interactor.onOpenTab(tab)
            }

            holder.itemView.mozac_browser_tabstray_close.setOnClickListener {
                interactor.onCloseTab(tab)
            }
        }
    }
}
