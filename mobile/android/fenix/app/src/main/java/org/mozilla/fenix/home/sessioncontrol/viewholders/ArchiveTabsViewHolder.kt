/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.archive_tabs_button.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.onNext

class ArchiveTabsViewHolder(
    view: View,
    private val actionEmitter: Observer<SessionControlAction>
) : RecyclerView.ViewHolder(view) {

    init {
        view.save_session_button.setOnClickListener {
            actionEmitter.onNext(TabAction.Archive)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.archive_tabs_button
    }
}
