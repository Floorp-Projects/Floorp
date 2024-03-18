/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ui.search

import android.view.View
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.response.dataset.usernamePresentationOrFallback

/**
 * ViewHolder for a login item in the autofill search view.
 */
internal class LoginViewHolder(
    itemView: View,
    private val onLoginSelected: (Login) -> Unit,
) : RecyclerView.ViewHolder(itemView) {
    private val usernameView = itemView.findViewById<TextView>(R.id.mozac_feature_autofill_username)
    private val originView = itemView.findViewById<TextView>(R.id.mozac_feature_autofill_origin)

    fun bind(login: Login) {
        usernameView.text = login.usernamePresentationOrFallback(itemView.context)
        originView.text = login.origin

        itemView.setOnClickListener { onLoginSelected(login) }
    }
}
