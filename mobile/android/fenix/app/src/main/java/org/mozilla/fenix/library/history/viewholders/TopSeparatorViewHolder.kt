/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.library.history.HistoryAdapter
import org.mozilla.fenix.R

/**
 * A view used as an extra space at the top of history and synced history lists.
 * [HistoryAdapter] is responsible for creating this view.
 *
 * @param view that is passed down to the parent's constructor.
 */
class TopSeparatorViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    companion object {
        const val LAYOUT_ID = R.layout.history_list_top_separator
    }
}
