/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.coroutines.Job
import org.mozilla.fenix.home.sessioncontrol.viewholders.ArchiveTabsViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.DeleteTabsViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.PrivateBrowsingDescriptionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.SessionHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.SessionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.SessionPlaceholderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabHeaderViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabViewHolder
import java.lang.IllegalStateException

sealed class AdapterItem {
    object TabHeader : AdapterItem()
    data class TabItem(val tab: Tab) : AdapterItem()
    object PrivateBrowsingDescription : AdapterItem()
    object ArchiveTabs : AdapterItem()
    object DeleteTabs : AdapterItem()
    object SessionHeader : AdapterItem()
    object SessionPlaceholder : AdapterItem()
    data class SessionItem(val session: ArchivedSession) : AdapterItem()

    val viewType: Int
        get() = when (this) {
            TabHeader -> TabHeaderViewHolder.LAYOUT_ID
            is TabItem -> TabViewHolder.LAYOUT_ID
            ArchiveTabs -> ArchiveTabsViewHolder.LAYOUT_ID
            PrivateBrowsingDescription -> PrivateBrowsingDescriptionViewHolder.LAYOUT_ID
            DeleteTabs -> DeleteTabsViewHolder.LAYOUT_ID
            SessionHeader -> SessionHeaderViewHolder.LAYOUT_ID
            SessionPlaceholder -> SessionPlaceholderViewHolder.LAYOUT_ID
            is SessionItem -> SessionViewHolder.LAYOUT_ID
        }
}

class SessionControlAdapter(
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    var items: List<AdapterItem> = listOf()
    private lateinit var job: Job

    fun reloadData(items: List<AdapterItem>) {
        this.items = items
        notifyDataSetChanged()
    }

    // This method triggers the ComplexMethod lint error when in fact it's quite simple.
    @SuppressWarnings("ComplexMethod")
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return when (viewType) {
            TabHeaderViewHolder.LAYOUT_ID -> TabHeaderViewHolder(view, actionEmitter)
            TabViewHolder.LAYOUT_ID -> TabViewHolder(view, actionEmitter, job)
            ArchiveTabsViewHolder.LAYOUT_ID -> ArchiveTabsViewHolder(view, actionEmitter)
            PrivateBrowsingDescriptionViewHolder.LAYOUT_ID -> PrivateBrowsingDescriptionViewHolder(view, actionEmitter)
            DeleteTabsViewHolder.LAYOUT_ID -> DeleteTabsViewHolder(view, actionEmitter)
            SessionHeaderViewHolder.LAYOUT_ID -> SessionHeaderViewHolder(view)
            SessionPlaceholderViewHolder.LAYOUT_ID -> SessionPlaceholderViewHolder(view)
            SessionViewHolder.LAYOUT_ID -> SessionViewHolder(view, actionEmitter)
            else -> throw IllegalStateException()
        }
    }

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        job = Job()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        job.cancel()
    }

    override fun getItemViewType(position: Int) = items[position].viewType

    override fun getItemCount(): Int = items.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is TabViewHolder -> holder.bindSession((items[position] as AdapterItem.TabItem).tab, position)
            is SessionViewHolder -> holder.bind((items[position] as AdapterItem.SessionItem).session)
        }
    }
}
