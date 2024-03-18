/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.viewholders

import android.view.View
import android.widget.TextView
import androidx.annotation.StringRes
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R

class ExceptionsHeaderViewHolder(
    view: View,
    @StringRes description: Int,
) : RecyclerView.ViewHolder(view) {

    init {
        view.findViewById<TextView>(R.id.exceptions_description).text =
            if (description == R.string.preferences_passwords_exceptions_description_2) {
                view.context.getString(description, view.context.getString(R.string.app_name))
            } else {
                view.context.getString(description)
            }
    }

    companion object {
        const val LAYOUT_ID = R.layout.exceptions_description
    }
}
