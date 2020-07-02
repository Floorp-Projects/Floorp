/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotectionexceptions.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.exception_item.view.*
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import org.mozilla.fenix.R
import org.mozilla.fenix.trackingprotectionexceptions.ExceptionsInteractor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView

/**
 * View holder for a single website that is exempted from Tracking Protection.
 */
class ExceptionsListItemViewHolder(
    view: View,
    private val interactor: ExceptionsInteractor
) : RecyclerView.ViewHolder(view) {

    private val favicon = view.favicon_image
    private val url = view.webAddressView
    private val deleteButton = view.delete_exception

    private var item: TrackingProtectionException? = null

    init {
        deleteButton.setOnClickListener {
            item?.let {
                interactor.onDeleteOne(it)
            }
        }
    }

    fun bind(item: TrackingProtectionException) {
        this.item = item
        url.text = item.url
        updateFavIcon(item.url)
    }

    private fun updateFavIcon(url: String) {
        favicon.context.components.core.icons.loadIntoView(favicon, url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.exception_item
    }
}
