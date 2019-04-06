/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.session_list_header.view.*
import org.mozilla.fenix.R

class SessionHeaderViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    val headerText = view.header_text

    init {
        headerText.text = "Today"
    }

    companion object {
        const val LAYOUT_ID = R.layout.session_list_header
    }
}
