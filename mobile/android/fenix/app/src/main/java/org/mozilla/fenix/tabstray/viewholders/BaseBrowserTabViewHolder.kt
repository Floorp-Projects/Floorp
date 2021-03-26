/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.View
import androidx.annotation.CallSuper
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.browser.BaseBrowserTrayList

/**
 * A shared view holder for browser tabs tray list.
 */
abstract class BaseBrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : AbstractTrayViewHolder(containerView) {

    protected val trayList: BaseBrowserTrayList = itemView.findViewById(R.id.tray_list_item)

    init {
        trayList.interactor = interactor
    }

    @CallSuper
    override fun bind(
        adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>,
        layoutManager: RecyclerView.LayoutManager
    ) {
        trayList.layoutManager = layoutManager
        trayList.adapter = adapter
    }
}
