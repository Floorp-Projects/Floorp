/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.view

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.logins_item.view.*
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.settings.logins.SavedLogin
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor

class LoginsListViewHolder(
    private val view: View,
    private val interactor: SavedLoginsInteractor
) : RecyclerView.ViewHolder(view) {

    private val favicon = view.favicon_image
    private val url = view.webAddressView
    private val username = view.usernameView
    private var loginItem: SavedLogin? = null

    fun bind(item: SavedLogin) {
        this.loginItem = SavedLogin(
            guid = item.guid,
            origin = item.origin,
            password = item.password,
            username = item.username,
            timeLastUsed = item.timeLastUsed
        )
        url.text = item.origin
        username.text = item.username

        updateFavIcon(item.origin)

        view.setOnClickListener {
            interactor.onItemClicked(item)
        }
    }

    private fun updateFavIcon(url: String) {
        favicon.context.components.core.icons.loadIntoView(favicon, url)
    }
}
