/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.no_content_message.view.*
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

class NoContentMessageViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {

    fun bind(
        @DrawableRes icon: Int,
        @StringRes header: Int,
        @StringRes description: Int
    ) {
        with(view.context) {
            view.no_content_header.putCompoundDrawablesRelativeWithIntrinsicBounds(end = getDrawable(icon))
            view.no_content_header.text = getString(header)
            view.no_content_description.text = getString(description)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.no_content_message
    }
}
