/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import kotlinx.android.synthetic.main.exception_item.*
import mozilla.components.browser.icons.BrowserIcons
import org.mozilla.fenix.R
import org.mozilla.fenix.exceptions.ExceptionsInteractor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for a single website that is exempted from Tracking Protection or Logins.
 */
class ExceptionsListItemViewHolder<T : Any>(
    view: View,
    private val interactor: ExceptionsInteractor<T>,
    private val icons: BrowserIcons = view.context.components.core.icons
) : ViewHolder(view) {

    private lateinit var item: T

    init {
        delete_exception.setOnClickListener {
            interactor.onDeleteOne(item)
        }
    }

    fun bind(item: T, url: String) {
        this.item = item
        webAddressView.text = url
        icons.loadIntoView(favicon_image, url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.exception_item
    }
}
