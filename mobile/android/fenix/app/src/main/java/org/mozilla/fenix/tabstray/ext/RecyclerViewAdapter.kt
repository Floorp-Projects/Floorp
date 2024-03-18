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
            // There's a bug where [onItemRangeInserted] is intermittently called with an [itemCount] of zero, causing
            // the Tabs Tray to always open scrolled at the top. This check forces [onItemRangeInserted] to wait
            // until [itemCount] is non-zero to execute [block] and remove the adapter observer.
            // This is a temporary fix until the Compose rewrite is enabled by default, where this bug is not present.
            if (itemCount > 0) {
                block.invoke()
                unregisterAdapterDataObserver(this)
            }
        }
    }
    registerAdapterDataObserver(observer)
}
