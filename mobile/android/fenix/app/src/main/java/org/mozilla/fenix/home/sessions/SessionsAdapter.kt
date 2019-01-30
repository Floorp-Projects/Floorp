/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessions

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R

class SessionsAdapter : RecyclerView.Adapter<RecyclerView.ViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            HeaderViewHolder.LAYOUT_ID -> HeaderViewHolder(view)
            EmptyListViewHolder.LAYOUT_ID -> EmptyListViewHolder(view)
            else -> EmptyListViewHolder(view)
        }
    }

    override fun getItemViewType(position: Int) = when (position) {
        0 -> HeaderViewHolder.LAYOUT_ID
        1 -> EmptyListViewHolder.LAYOUT_ID
        else -> -1
    }

    override fun getItemCount(): Int = 2

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        if (holder is HeaderViewHolder) {
            holder.headerText.text = "Today"
        }
    }

    private class HeaderViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {
        val headerText = view.findViewById<TextView>(R.id.header_text)

        companion object {
            const val LAYOUT_ID = R.layout.session_list_header
        }
    }

    private class EmptyListViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {
        companion object {
            const val LAYOUT_ID = R.layout.session_list_empty
        }
    }
}
