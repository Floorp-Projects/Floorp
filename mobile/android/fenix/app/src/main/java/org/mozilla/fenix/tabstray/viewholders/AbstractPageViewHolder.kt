/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.tabstray.TrayPagerAdapter

/**
 * An abstract [RecyclerView.ViewHolder] for [TrayPagerAdapter] items.
 */
abstract class AbstractPageViewHolder constructor(
    val containerView: View
) : RecyclerView.ViewHolder(containerView) {

    abstract fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>
    )
}
