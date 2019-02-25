/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.graphics.PorterDuff
import android.text.SpannableString
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.session_item.view.*
import org.mozilla.fenix.R

class SessionsAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    private var items = listOf<ArchivedSession>()


    fun reloadDatat(items: List<ArchivedSession>) {
        this.items = items
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            HeaderViewHolder.LAYOUT_ID -> HeaderViewHolder(view)
            EmptyListViewHolder.LAYOUT_ID -> EmptyListViewHolder(view)
            SessionItemViewHolder.LAYOUT_ID -> SessionItemViewHolder(view)
            PrivateEmptyListViewHolder.LAYOUT_ID -> PrivateEmptyListViewHolder(view)
            else -> EmptyListViewHolder(view)
        }
    }

    override fun getItemViewType(position: Int) = when (position) {
        0 -> HeaderViewHolder.LAYOUT_ID
        else -> SessionItemViewHolder.LAYOUT_ID
    }

    override fun getItemCount(): Int = items.size + 1

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        when (holder) {
            is HeaderViewHolder -> holder.headerText.text = "Today"
            is SessionItemViewHolder -> holder.bind(items[position - 1])
            is PrivateEmptyListViewHolder -> {
                // Format the description text to include a hyperlink
                val descriptionText = String
                    .format(holder.description.text.toString(), System.getProperty("line.separator"))
                val linkStartIndex = descriptionText.indexOf("\n\n") + 2
                val linkAction = object : ClickableSpan() {
                    override fun onClick(widget: View?) {
                        // TODO Go to SUMO page
                    }
                }
                val textWithLink = SpannableString(descriptionText).apply {
                    setSpan(linkAction, linkStartIndex, descriptionText.length, 0)

                    val colorSpan = ForegroundColorSpan(holder.description.currentTextColor)
                    setSpan(colorSpan, linkStartIndex, descriptionText.length, 0)
                }
                holder.description.text = textWithLink
            }
        }
    }

    private class HeaderViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val headerText = view.findViewById<TextView>(R.id.header_text)
        companion object {
            const val LAYOUT_ID = R.layout.session_list_header
        }
    }

    private class PrivateEmptyListViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val description = view.findViewById<TextView>(R.id.session_description)
        companion object {
            const val LAYOUT_ID = R.layout.session_list_empty_private
        }
    }

    private class SessionItemViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        private val thumbnail = view.session_card_thumbnail
        private val timestampLabel = view.session_card_timestamp
        private val titlesLabel = view.session_card_titles
        private val extrasLabel = view.session_card_extras

        private var session: ArchivedSession? = null

        fun bind(session: ArchivedSession) {
            this.session = session

            thumbnail.setColorFilter(
                ContextCompat.getColor(itemView.context, AVAILABLE_COLOR_IDS.random()),
                PorterDuff.Mode.MULTIPLY)
            timestampLabel.text = session.formattedSavedAt
            titlesLabel.text = session.titles
            extrasLabel.text = if (session.extrasLabel > 0) {
                "+${session.extrasLabel} sites..."
            } else { "" }
        }

        companion object {
            private val AVAILABLE_COLOR_IDS = listOf(
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
