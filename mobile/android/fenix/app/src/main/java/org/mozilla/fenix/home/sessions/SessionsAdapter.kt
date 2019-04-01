/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.graphics.PorterDuff
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.session_item.*
import org.mozilla.fenix.R

class SessionsAdapter(
    private val actionEmitter: Observer<SessionsAction>
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    sealed class SessionListState {
        data class DisplaySessions(val sessions: List<ArchivedSession>) : SessionListState()
        object Empty : SessionListState()

        val items: List<ArchivedSession>
            get() = when (this) {
                is DisplaySessions -> this.sessions
                is Empty -> listOf()
            }

        val size: Int
            get() = when (this) {
                is DisplaySessions -> this.sessions.size
                is Empty -> EMPTY_SIZE
            }

        companion object {
            private const val EMPTY_SIZE = 1
        }
    }

    private var state: SessionListState = SessionListState.Empty

    fun reloadData(items: List<ArchivedSession>) {
        this.state = if (items.isEmpty()) {
            SessionListState.Empty
        } else {
            SessionListState.DisplaySessions(items)
        }

        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            HeaderViewHolder.LAYOUT_ID -> HeaderViewHolder(view)
            EmptyListViewHolder.LAYOUT_ID -> EmptyListViewHolder(view)
            SessionItemViewHolder.LAYOUT_ID -> SessionItemViewHolder(view, actionEmitter)
            else -> EmptyListViewHolder(view)
        }
    }

    override fun getItemViewType(position: Int) = when (position) {
        0 -> HeaderViewHolder.LAYOUT_ID
        else -> if (state is SessionListState.DisplaySessions) {
            SessionItemViewHolder.LAYOUT_ID
        } else {
            EmptyListViewHolder.LAYOUT_ID
        }
    }

    override fun getItemCount(): Int = state.size + 1

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is HeaderViewHolder -> holder.headerText.text = "Today"
            is SessionItemViewHolder -> holder.bind(state.items[position - 1])
        }
    }

    private class HeaderViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val headerText = view.findViewById<TextView>(R.id.header_text)
        companion object {
            const val LAYOUT_ID = R.layout.session_list_header
        }
    }

    private class SessionItemViewHolder(
        view: View,
        private val actionEmitter: Observer<SessionsAction>,
        override val containerView: View? = view
    ) : RecyclerView.ViewHolder(view), LayoutContainer {
        private var session: ArchivedSession? = null

        init {
            session_item.setOnClickListener {
                session?.apply { actionEmitter.onNext(SessionsAction.Select(this)) }
            }

            session_card_overflow_button.setOnClickListener {
                session?.apply { actionEmitter.onNext(SessionsAction.MenuTapped(this)) }
            }

            session_card_share_button.setOnClickListener {
                session?.apply { actionEmitter.onNext(SessionsAction.ShareTapped(this)) }
            }
        }

        fun bind(session: ArchivedSession) {
            this.session = session
            val color = availableColors[(session.id % availableColors.size).toInt()]

            session_card_thumbnail.setColorFilter(
                ContextCompat.getColor(itemView.context, color),
                PorterDuff.Mode.MULTIPLY)
            session_card_timestamp.text = session.formattedSavedAt
            session_card_titles.text = session.titles
            session_card_extras.text = if (session.extrasLabel > 0) {
                "+${session.extrasLabel} sites..."
            } else { "" }
        }

        companion object {
            private val availableColors = listOf(
                R.color.photonBlue40, R.color.photonGreen50, R.color.photonYellow50, R.color.photonOrange50,
                R.color.photonPurple50, R.color.photonInk70)
            const val LAYOUT_ID = R.layout.session_item
        }
    }

    private class EmptyListViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        companion object {
            const val LAYOUT_ID = R.layout.session_list_empty
        }
    }
}
