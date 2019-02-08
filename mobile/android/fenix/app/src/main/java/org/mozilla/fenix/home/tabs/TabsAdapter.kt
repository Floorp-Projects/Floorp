/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tabs

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_list_row.*
import mozilla.components.browser.session.Session
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea

class TabsAdapter(private val actionEmitter: Observer<TabsAction>) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    var sessions = listOf<Session>()
        set(value) {
            val diffResult = DiffUtil.calculateDiff(TabsDiffCallback(field, value), true)
            field = value
            diffResult.dispatchUpdatesTo(this@TabsAdapter)
        }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return TabViewHolder(view, actionEmitter)
    }

    override fun getItemViewType(position: Int) = TabViewHolder.LAYOUT_ID

    override fun getItemCount(): Int = sessions.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is TabViewHolder -> {
                holder.bindSession(sessions[position])
            }
        }
    }

    private class TabViewHolder(
        view: View,
        actionEmitter: Observer<TabsAction>,
        override val containerView: View? = view
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer {

        var session: Session? = null

        init {
            item_tab.setOnClickListener {
                actionEmitter.onNext(TabsAction.Select(session!!))
            }

            close_tab_button?.run {
                increaseTapArea(closeButtonIncreaseDps)
                setOnClickListener {
                    actionEmitter.onNext(TabsAction.Close(session!!))
                }
            }
        }

        fun bindSession(session: Session) {
            this.session = session
            text_url.text = session.url
        }

        companion object {
            const val closeButtonIncreaseDps = 12
            const val LAYOUT_ID = R.layout.tab_list_row
        }
    }
}

class TabsDiffCallback(
    private val oldList: List<Session>,
    private val newList: List<Session>
) : DiffUtil.Callback() {

    override fun getOldListSize(): Int = oldList.size
    override fun getNewListSize(): Int = newList.size

    override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
        return oldList[oldItemPosition].id == newList[newItemPosition].id
    }

    override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
        return oldList[oldItemPosition] == newList[newItemPosition]
    }

    override fun getChangePayload(oldItemPosition: Int, newItemPosition: Int): Any? {
        val oldSession = oldList[oldItemPosition]
        val newSession = newList[newItemPosition]
        val diffBundle = Bundle()
        if (oldSession.url != newSession.url) {
            diffBundle.putString("url", newSession.url)
        }
        return if (diffBundle.size() == 0) null else diffBundle
    }
}
