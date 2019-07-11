/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.delete_history_button.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryState

class HistoryDeleteButtonViewHolder(
    view: View,
    historyInteractor: HistoryInteractor
) : RecyclerView.ViewHolder(view) {
    private var mode: HistoryState.Mode? = null
    private val buttonView = view.delete_history_button

    init {
        buttonView.setOnClickListener {
            mode?.also {
                when (it) {
                    is HistoryState.Mode.Normal -> historyInteractor.onDeleteAll()
                    is HistoryState.Mode.Editing -> historyInteractor.onDeleteSome(it.selectedItems)
                }
            }
        }
    }

    fun bind(mode: HistoryState.Mode) {
        this.mode = mode

        buttonView.run {
            val isDeleting = mode is HistoryState.Mode.Deleting
            if (isDeleting || mode is HistoryState.Mode.Editing && mode.selectedItems.isNotEmpty()) {
                isEnabled = false
                alpha = DISABLED_ALPHA
            } else {
                isEnabled = true
                alpha = 1f
            }
        }
    }

    companion object {
        const val DISABLED_ALPHA = 0.4f
        const val LAYOUT_ID = R.layout.delete_history_button
    }
}
