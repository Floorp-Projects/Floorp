/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.View
import androidx.recyclerview.widget.RecyclerView

/**
 * View holders that extend this base class are passed both the new value and last value
 * when [bind] is called. Use this information to diff the changes between the two values.
 */
internal abstract class LastItemViewHolder<T>(
    itemView: View,
) : RecyclerView.ViewHolder(itemView) {

    protected var lastCandidate: T? = null

    /**
     * Updates the held view to reflect changes in the menu option.
     *
     * @param newCandidate New value to use.
     * @param oldCandidate Previously set value.
     * If this is the first time [bind] was called, null is passed.
     */
    protected abstract fun bind(newCandidate: T, oldCandidate: T?)

    fun bind(option: T) {
        bind(option, lastCandidate)
        lastCandidate = option
    }
}
