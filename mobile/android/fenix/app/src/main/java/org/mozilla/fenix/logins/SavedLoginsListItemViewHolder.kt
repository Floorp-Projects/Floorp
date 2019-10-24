/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.logins

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.logins_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView

class SavedLoginsListItemViewHolder(
    private val view: View,
    private val interactor: SavedLoginsInteractor
) : RecyclerView.ViewHolder(view) {

    private val favicon = view.favicon_image
    private val url = view.domainView
    private val userName = view.userView

    private var item: SavedLoginsItem? = null

    fun bind(item: SavedLoginsItem) {
        this.item = item
        url.text = item.url
        userName.text = item.userName
        updateFavIcon(item.url)
        view.setOnClickListener {
            interactor.itemClicked(item)
        }
    }

    private fun updateFavIcon(url: String) {
        favicon.context.components.core.icons.loadIntoView(favicon, url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.logins_item
    }
}
