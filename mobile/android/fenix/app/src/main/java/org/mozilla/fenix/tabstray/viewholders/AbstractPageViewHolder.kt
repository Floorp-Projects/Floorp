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
    val containerView: View,
) : RecyclerView.ViewHolder(containerView) {

    /**
     * Invoked when the nested [RecyclerView.Adapter] is bound to the [RecyclerView.ViewHolder].
     */
    abstract fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
    )

    /**
     * Invoked when the [RecyclerView.ViewHolder] is attached from the window. This could have
     * previously been bound and is now attached again.
     */
    abstract fun attachedToWindow()

    /**
     * Invoked when the [RecyclerView.ViewHolder] is detached from the window.
     */
    abstract fun detachedFromWindow()
}
