/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import android.widget.TextView
import androidx.annotation.CallSuper
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.TrayPagerAdapter
import org.mozilla.fenix.tabstray.browser.AbstractBrowserTrayList

/**
 * A shared view holder for browser tabs tray list.
 */
abstract class AbstractBrowserPageViewHolder(
    containerView: View,
    tabsTrayStore: TabsTrayStore,
    interactor: TabsTrayInteractor,
) : AbstractPageViewHolder(containerView) {

    private val trayList: AbstractBrowserTrayList = itemView.findViewById(R.id.tray_list_item)
    private val emptyList: TextView = itemView.findViewById(R.id.tab_tray_empty_view)
    private var adapterObserver: RecyclerView.AdapterDataObserver? = null
    private var adapterRef: RecyclerView.Adapter<out RecyclerView.ViewHolder>? = null

    abstract val emptyStringText: String

    init {
        trayList.interactor = interactor
        trayList.tabsTrayStore = tabsTrayStore
        emptyList.text = emptyStringText
    }

    /**
     * A way for an implementor of [AbstractBrowserPageViewHolder] to define their own scroll-to-tab behaviour.
     */
    abstract fun scrollToTab(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager,
    )

    @CallSuper
    protected fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager,
    ) {
        adapterRef = adapter

        scrollToTab(adapter, layoutManager)

        trayList.layoutManager = layoutManager
        trayList.adapter = adapter
    }

    /**
     * When the [RecyclerView.Adapter] is attached to the window we register a data observer to
     * always check whether to call [updateTrayVisibility].
     *
     * We keep a constant observer instead of using [RecyclerView.Adapter.observeFirstInsert],
     * because some adapters can insert empty lists and trigger the one-shot observer too soon.
     *
     * See also [AbstractPageViewHolder.attachedToWindow].
     */
    override fun attachedToWindow() {
        adapterRef?.let { adapter ->
            adapterObserver = object : RecyclerView.AdapterDataObserver() {
                override fun onItemRangeInserted(positionStart: Int, itemCount: Int) {
                    updateTrayVisibility(showTrayList(adapter))
                }

                override fun onItemRangeRemoved(positionstart: Int, itemcount: Int) {
                    updateTrayVisibility(showTrayList(adapter))
                }
            }
            adapterObserver?.let {
                adapter.registerAdapterDataObserver(it)
            }
        }
    }

    /**
     * [RecyclerView.AdapterDataObserver]s are responsible to be unregistered when they are done,
     * so we do that here when our [TrayPagerAdapter] page is detached from the window.
     *
     * See also [AbstractPageViewHolder.detachedFromWindow].
     */
    override fun detachedFromWindow() {
        adapterObserver?.let {
            adapterRef?.unregisterAdapterDataObserver(it)
            adapterObserver = null
        }
    }

    /**
     * A way for an implementor of [AbstractBrowserPageViewHolder] to define their own behavior of
     * when to show/hide the tray list and empty list UI.
     */
    open fun showTrayList(adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>): Boolean =
        adapter.itemCount > 0

    /**
     * Helper function used to toggle the visibility of the tabs tray lists and the empty list message.
     */
    fun updateTrayVisibility(showTrayList: Boolean) {
        trayList.isVisible = showTrayList
        emptyList.isVisible = !showTrayList
    }
}
