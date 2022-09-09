/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView

/**
 * A GridLayoutManager that can be used to override methods in Android implementation
 * to improve ayy1 or fix a11y issues.
 */
class AccessibilityGridLayoutManager(
    context: Context,
    spanCount: Int,
) : GridLayoutManager(
    context,
    spanCount,
) {
    override fun getColumnCountForAccessibility(
        recycler: RecyclerView.Recycler,
        state: RecyclerView.State,
    ): Int {
        return if (itemCount < spanCount) {
            itemCount
        } else {
            super.getColumnCountForAccessibility(recycler, state)
        }
    }
}
