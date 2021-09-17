/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import androidx.recyclerview.widget.RecyclerView

/**
 * Observes the adapter and invokes the callback [block] only when data is first inserted to the adapter.
 */
fun <VH : RecyclerView.ViewHolder> RecyclerView.Adapter<out VH>.observeFirstInsert(block: () -> Unit) {
    val observer = object : RecyclerView.AdapterDataObserver() {
        override fun onItemRangeInserted(positionStart: Int, itemCount: Int) {
            block.invoke()
            unregisterAdapterDataObserver(this)
        }
    }
    registerAdapterDataObserver(observer)
}
