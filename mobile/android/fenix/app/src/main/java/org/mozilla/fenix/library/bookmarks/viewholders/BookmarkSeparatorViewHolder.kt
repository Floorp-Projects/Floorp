/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R

/**
 * Simple view holder for dividers in the bookmarks list.
 */
class BookmarkSeparatorViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    companion object {
        const val LAYOUT_ID = R.layout.library_separator
    }
}
