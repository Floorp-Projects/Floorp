/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tabs

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.tab_list_row.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import org.jetbrains.anko.image
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import kotlin.coroutines.CoroutineContext

class TabsAdapter(private val actionEmitter: Observer<TabsAction>) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    lateinit var job: Job

    var sessions = listOf<SessionViewState>()
        set(value) {
            val diffResult = DiffUtil.calculateDiff(TabsDiffCallback(field, value), true)
            field = value
            diffResult.dispatchUpdatesTo(this@TabsAdapter)
        }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return TabViewHolder(view, actionEmitter, job)
    }

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        job = Job()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        job.cancel()
    }

    override fun getItemViewType(position: Int) = TabViewHolder.LAYOUT_ID

    override fun getItemCount(): Int = sessions.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is TabViewHolder -> {
                holder.bindSession(sessions[position], position)
            }
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int, payloads: MutableList<Any>) {
        if (payloads.isEmpty()) onBindViewHolder(holder, position)
        else if (holder is TabViewHolder) {
            val bundle = payloads[0] as Bundle
            bundle.getString(tab_url)?.apply(holder::updateUrl)
            bundle.getBoolean(tab_selected).apply(holder::updateSelected)
        }
    }

    private class TabViewHolder(
        val view: View,
        actionEmitter: Observer<TabsAction>,
        val job: Job,
        override val containerView: View? = view
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer, CoroutineScope {

        override val coroutineContext: CoroutineContext
            get() = Dispatchers.IO + job

        var session: SessionViewState? = null

        init {
            item_tab.setOnClickListener {
                actionEmitter.onNext(TabsAction.Select(session?.id!!))
            }

            close_tab_button?.run {
                increaseTapArea(closeButtonIncreaseDps)
                setOnClickListener {
                    actionEmitter.onNext(TabsAction.Close(session?.id!!))
                }
            }
        }

        fun bindSession(session: SessionViewState, position: Int) {
            this.session = session
            updateTabBackground(position)
            updateUrl(session.url)
            updateSelected(session.selected)
        }

        fun updateUrl(url: String) {
            text_url.text = url
            launch(IO) {
                val bitmap = favicon_image.context.components.utils.icons
                    .loadIcon(IconRequest(url)).await().bitmap
                launch(Main) {
                    favicon_image.setImageBitmap(bitmap)
                }
            }
        }

        fun updateSelected(selected: Boolean) {
            selected_border.visibility = if (selected) View.VISIBLE else View.GONE
        }

        fun updateTabBackground(id: Int) {
            if (session?.thumbnail != null) {
                tab_background.setImageBitmap(session?.thumbnail)
            } else {
                val background = availableBackgrounds[id % availableBackgrounds.size]
                tab_background.image = ContextCompat.getDrawable(view.context, background)
            }
        }

        companion object {
            const val closeButtonIncreaseDps = 12
            const val LAYOUT_ID = R.layout.tab_list_row
        }
    }

    companion object {
        const val tab_url = "url"
        const val tab_selected = "selected"
        private val availableBackgrounds = listOf(R.drawable.sessions_01, R.drawable.sessions_02,
            R.drawable.sessions_03, R.drawable.sessions_04, R.drawable.sessions_05, R.drawable.sessions_06,
            R.drawable.sessions_07, R.drawable.sessions_08)
    }
}

class TabsDiffCallback(
    private val oldList: List<SessionViewState>,
    private val newList: List<SessionViewState>
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
            diffBundle.putString(TabsAdapter.tab_url, newSession.url)
        }
        if (oldSession.selected != newSession.selected) {
            diffBundle.putBoolean(TabsAdapter.tab_selected, newSession.selected)
        }
        return if (diffBundle.size() == 0) null else diffBundle
    }
}
