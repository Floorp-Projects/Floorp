/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.View
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BaseBrowserTrayList

sealed class TrayViewHolder constructor(
    override val containerView: View
) : RecyclerView.ViewHolder(containerView), LayoutContainer {

    abstract fun bind(adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>)
}

class BrowserTabViewHolder(
    containerView: View,
    interactor: TabsTrayInteractor
) : TrayViewHolder(containerView) {

    private val trayList: BaseBrowserTrayList = itemView.findViewById(R.id.tray_list_item)

    init {
        trayList.interactor = interactor
    }

    override fun bind(adapter: RecyclerView.Adapter<out RecyclerView.ViewHolder>) {
        trayList.layoutManager = LinearLayoutManager(itemView.context)
        trayList.adapter = adapter
    }

    companion object {
        const val LAYOUT_ID_NORMAL_TAB = R.layout.normal_browser_tray_list
        const val LAYOUT_ID_PRIVATE_TAB = R.layout.private_browser_tray_list
    }
}
