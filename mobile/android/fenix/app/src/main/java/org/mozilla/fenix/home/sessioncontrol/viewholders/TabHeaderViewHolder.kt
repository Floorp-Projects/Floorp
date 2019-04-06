/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.tab_header.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext

class TabHeaderViewHolder(
    view: View,
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.ViewHolder(view) {
    private var isPrivate = false

    init {
        view.apply {
            add_tab_button.increaseTapArea(addTabButtonIncreaseDps)

            add_tab_button.setOnClickListener {
                actionEmitter.onNext(TabAction.Add)
            }

            val headerTextResourceId = if (isPrivate) R.string.tabs_header_private_title else R.string.tabs_header_title
            header_text.text = context.getString(headerTextResourceId)
            tabs_overflow_button.increaseTapArea(overflowButtonIncreaseDps)
            tabs_overflow_button.setOnClickListener {
                actionEmitter.onNext(TabAction.MenuTapped)
            }
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.tab_header

        const val addTabButtonIncreaseDps = 8
        const val overflowButtonIncreaseDps = 8
    }
}
