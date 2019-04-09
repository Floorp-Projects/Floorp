/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.delete_history_button.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.library.history.HistoryAction
import org.mozilla.fenix.library.history.HistoryState

class HistoryDeleteButtonViewHolder(
    view: View,
    private val actionEmitter: Observer<HistoryAction>
) : RecyclerView.ViewHolder(view) {
    private var mode: HistoryState.Mode? = null
    private val textView = view.delete_history_button_text
    private val buttonView = view.delete_history_button

    init {
        buttonView.setOnClickListener {
            mode?.also {
                val action = when (it) {
                    is HistoryState.Mode.Normal -> HistoryAction.Delete.All
                    is HistoryState.Mode.Editing -> HistoryAction.Delete.Some(it.selectedItems)
                }

                actionEmitter.onNext(action)
            }
        }
    }
    fun bind(mode: HistoryState.Mode) {
        val mode = mode

        val text = if (mode is HistoryState.Mode.Editing && mode.selectedItems.isNotEmpty()) {
            textView.context.resources.getString(
                R.string.history_delete_some,
                mode.selectedItems.size
            )
        } else {
            textView.context.resources.getString(R.string.history_delete_all)
        }

        buttonView.contentDescription = text
        textView.text = text
    }

    companion object {
        const val LAYOUT_ID = R.layout.delete_history_button
    }
}
