/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    private val buttonView = view.delete_history_button

    init {
        buttonView.setOnClickListener {
            mode?.also {
                val action = when (it) {
                    is HistoryState.Mode.Normal -> HistoryAction.Delete.All
                    is HistoryState.Mode.Editing -> HistoryAction.Delete.Some(it.selectedItems)
                    is HistoryState.Mode.Deleting -> null
                } ?: return@also

                actionEmitter.onNext(action)
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
