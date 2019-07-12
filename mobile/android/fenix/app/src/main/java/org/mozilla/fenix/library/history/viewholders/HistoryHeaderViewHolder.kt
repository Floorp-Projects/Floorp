/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.history_header.view.*
import org.mozilla.fenix.R

class HistoryHeaderViewHolder(
    view: View
) : RecyclerView.ViewHolder(view) {
    private val title = view.history_header_title

    fun bind(title: String) {
        this.title.text = title
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_header
    }
}
