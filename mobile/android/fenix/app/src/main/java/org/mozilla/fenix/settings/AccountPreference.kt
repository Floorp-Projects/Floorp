/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R

class AccountPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    attributeSetId: Int = android.R.attr.preferenceStyle
) : Preference(context, attrs, attributeSetId) {
    var displayName: String? = null
    var email: String? = null

    init {
        layoutResource = R.layout.account_preference
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)
        val displayNameView = holder.findViewById(R.id.displayName) as TextView
        val emailView = holder.findViewById(R.id.email) as TextView

        displayNameView.text = displayName.orEmpty()
        displayNameView.visibility = when (displayName.isNullOrEmpty()) {
            true -> View.GONE
            false -> View.VISIBLE
        }
        // There is a potential for a race condition here. We might not have the user profile by the time we display
        // this field, in which case we won't have the email address (or the display name, but that we may just not have
        // at all even after fetching the profile). We don't hide the email field or change its text if email is missing
        // because in the layout a default value ("Firefox Account") is specified, which will be displayed instead.
        email?.let { emailView.text = it }
    }
}
