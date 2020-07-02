/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.loginexceptions.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.exception_item.view.*
import mozilla.components.feature.logins.exceptions.LoginException
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.loginexceptions.LoginExceptionsInteractor

/**
 * View holder for a single website that is exempted from Tracking Protection.
 */
class LoginExceptionsListItemViewHolder(
    view: View,
    private val interactor: LoginExceptionsInteractor
) : RecyclerView.ViewHolder(view) {

    private val favicon = view.favicon_image
    private val url = view.webAddressView
    private val deleteButton = view.delete_exception

    private var item: LoginException? = null

    init {
        deleteButton.setOnClickListener {
            item?.let {
                interactor.onDeleteOne(it)
            }
        }
    }

    fun bind(item: LoginException) {
        this.item = item
        url.text = item.origin
    }

    private fun updateFavIcon(url: String) {
        favicon.context.components.core.icons.loadIntoView(favicon, url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.exception_item
    }
}
