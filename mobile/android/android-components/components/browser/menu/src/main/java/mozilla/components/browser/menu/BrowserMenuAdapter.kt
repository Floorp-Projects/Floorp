/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import android.graphics.Color
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu.view.StickyItemsAdapter

/**
 * Adapter implementation used by the browser menu to display menu items in a RecyclerView.
 */
internal class BrowserMenuAdapter(
    context: Context,
    items: List<BrowserMenuItem>,
) : RecyclerView.Adapter<BrowserMenuItemViewHolder>(), StickyItemsAdapter {
    var menu: BrowserMenu? = null

    internal val visibleItems = items.filter { it.visible() }
    internal val interactiveCount = visibleItems.sumOf { it.interactiveCount() }
    private val inflater = LayoutInflater.from(context)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int) =
        BrowserMenuItemViewHolder(inflater.inflate(viewType, parent, false))

    override fun getItemCount() = visibleItems.size

    override fun getItemViewType(position: Int): Int = visibleItems[position].getLayoutResource()

    override fun onBindViewHolder(holder: BrowserMenuItemViewHolder, position: Int) {
        visibleItems[position].bind(menu!!, holder.itemView)
    }

    fun invalidate(recyclerView: RecyclerView) {
        visibleItems.withIndex().forEach {
            val (index, item) = it
            recyclerView.findViewHolderForAdapterPosition(index)?.apply {
                item.invalidate(itemView)
            }
        }
    }

    @Suppress("TooGenericExceptionCaught")
    override fun isStickyItem(position: Int): Boolean {
        return try {
            visibleItems[position].isSticky
        } catch (e: IndexOutOfBoundsException) {
            false
        }
    }

    override fun setupStickyItem(stickyItem: View) {
        menu?.let {
            stickyItem.setBackgroundColor(it.backgroundColor)
        }
    }

    override fun tearDownStickyItem(stickyItem: View) {
        stickyItem.setBackgroundColor(Color.TRANSPARENT)
    }
}

class BrowserMenuItemViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView)
